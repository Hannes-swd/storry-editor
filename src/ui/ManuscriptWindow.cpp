// Manuskript-Fenster: hier wird die Geschichte geschrieben. Die Struktur
// entsteht nebenbei aus Marken im Text (siehe core/Manuscript.h).
//
// Leitgedanke der Oberflaeche: niemand muss die Marken auswendig koennen. Die
// Leiste oben ist in vier Gruppen geteilt - was der Text *ist* (Schreiben /
// Lesen), was *hineinkommt* (Einfuegen), wie es *aussieht* (Format) und wie
// man etwas *wiederfindet* (Suchen). Rechts steht, wo die Geschichte gerade
// steht: Zeitpunkt, Wortzahl, Speicherstand.
#include <algorithm>
#include <string>
#include <vector>

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_stdlib.h"

#include "core/Manuscript.h"
#include "core/StoryTime.h"
#include "ui/Dialogs.h"
#include "ui/Editor.h"
#include "ui/Lang.h"
#include "ui/Theme.h"
#include "ui/UiCommon.h"
#include "ui/Windows.h"

namespace se {
namespace {

// Jede Schaltflaeche der Leiste aendert den Text an der Cursorstelle. Weil das
// Textfeld beim Klick die Eingabe verliert, wird der Wunsch gemerkt und im
// naechsten Frame ueber den Callback ausgefuehrt - erst dann steht der Cursor
// wieder dort, wo der Benutzer ihn gelassen hat.
enum class EditRequest {
    None,
    Insert,      // Text an der Cursorstelle einsetzen
    Wrap,        // Auswahl (oder Cursor) mit Zeichen umschliessen
    LinePrefix,  // Zeilenanfang voranstellen, z.B. "## "
    Select,      // Cursor setzen und Bereich markieren
    Replace,     // Bereich durch anderen Text ersetzen
    ReplaceAll,  // den ganzen Text austauschen
};

struct ManuscriptState {
    bool readMode = false;
    bool showOutline = true;
    bool showMarks = true;  // Zeit- und Aktionsmarken: nur Ansicht, nie im Text
    bool showHelp = false;

    std::string actionSearch;
    std::string elementSearch;
    std::string timeDraft;

    int cursor = 0;      // letzte bekannte Cursorposition
    int selBegin = 0;    // letzte bekannte Auswahl
    int selEnd = 0;

    // ------------------------------------------------------------- Suchen
    bool showFind = false;
    bool findFocus = false;
    std::string findQuery;
    std::string replaceQuery;
    bool findCase = false;
    int findHit = 0;  // welcher Treffer gerade angesteuert wird

    // ------------------------------------------------- Auftrag ans Textfeld
    EditRequest request = EditRequest::None;
    std::string requestText;    // Insert / Replace / ReplaceAll / Wrap-Anfang
    std::string requestSuffix;  // Wrap-Ende
    int requestBegin = 0;
    int requestEnd = 0;
    bool focusText = false;      // Eingabe zurueck ins Textfeld holen
    bool restoreCursor = false;  // ... und den Cursor dorthin, wo er war

    // ---------------------------------------------- Autovervollstaendigung
    std::string completionWord;  // gerade getipptes "@..." oder "!..."
    char completionSigil = 0;    // '@' oder '!'
    int completionStart = -1;
    int completionPick = 0;
    bool completionOpen = false;   // Vorschlagsfenster stand im letzten Frame offen
    bool pickFiltered = false;     // es wurde etwas getippt, das die Liste einengt
    bool pickMoved = false;        // mit den Pfeilen bewusst ausgewaehlt
    std::string pickText;          // was beim Uebernehmen eingesetzt wird
    bool acceptRequested = false;  // Enter/Tab gedrueckt, der Callback ersetzt
    bool muted = false;            // mit Esc weggeklickt oder gerade uebernommen
    std::string mutedWord;         // ... und zwar fuer genau dieses Wort
};

ManuscriptState& state() {
    static ManuscriptState s;
    return s;
}

// Anfang der Zeile, in der `pos` liegt.
int lineBegin(const char* buf, int length, int pos) {
    int i = std::min(pos, length);
    while (i > 0 && buf[i - 1] != '\n') --i;
    return i;
}

// Callback: merkt sich Cursor und Auswahl und fuehrt den gemerkten Auftrag aus.
int editCallback(ImGuiInputTextCallbackData* data) {
    ManuscriptState& st = *static_cast<ManuscriptState*>(data->UserData);
    if (data->EventFlag != ImGuiInputTextFlags_CallbackAlways) return 0;

    // Das Textfeld kommt gerade frisch aus der Leiste zurueck: Cursor wieder
    // dorthin, wo weitergeschrieben werden soll.
    if (st.restoreCursor) {
        const int at = std::min(std::max(st.cursor, 0), data->BufTextLen);
        data->CursorPos = at;
        data->SelectionStart = std::min(std::max(st.selBegin, 0), data->BufTextLen);
        data->SelectionEnd = std::min(std::max(st.selEnd, 0), data->BufTextLen);
        st.restoreCursor = false;
    }

    // Wer nur an eine Stelle gesprungen ist, hat nichts getippt: dann darf die
    // Vorschlagsliste nicht aufgehen, bloss weil der Cursor hinter "@Alice" steht.
    const bool jumped = st.request == EditRequest::Select || st.request == EditRequest::Replace ||
                        st.request == EditRequest::ReplaceAll;

    switch (st.request) {
        case EditRequest::Insert:
            data->InsertChars(data->CursorPos, st.requestText.c_str());
            break;
        case EditRequest::Wrap: {
            // Zuerst hinten einsetzen - sonst verschiebt der vordere Teil die
            // zweite Position.
            const int begin = std::min(std::max(st.requestBegin, 0), data->BufTextLen);
            const int end = std::min(std::max(st.requestEnd, begin), data->BufTextLen);
            data->InsertChars(end, st.requestSuffix.c_str());
            data->InsertChars(begin, st.requestText.c_str());
            data->CursorPos = end == begin ? begin + static_cast<int>(st.requestText.size())
                                           : end + static_cast<int>(st.requestText.size() +
                                                                    st.requestSuffix.size());
            data->SelectionStart = data->SelectionEnd = data->CursorPos;
            break;
        }
        case EditRequest::LinePrefix: {
            const int at = lineBegin(data->Buf, data->BufTextLen, data->CursorPos);
            data->InsertChars(at, st.requestText.c_str());
            break;
        }
        case EditRequest::Select: {
            const int begin = std::min(std::max(st.requestBegin, 0), data->BufTextLen);
            const int end = std::min(std::max(st.requestEnd, begin), data->BufTextLen);
            data->CursorPos = end;
            data->SelectionStart = begin;
            data->SelectionEnd = end;
            break;
        }
        case EditRequest::Replace: {
            const int begin = std::min(std::max(st.requestBegin, 0), data->BufTextLen);
            const int end = std::min(std::max(st.requestEnd, begin), data->BufTextLen);
            data->DeleteChars(begin, end - begin);
            data->InsertChars(begin, st.requestText.c_str());
            data->CursorPos = begin + static_cast<int>(st.requestText.size());
            data->SelectionStart = data->SelectionEnd = data->CursorPos;
            break;
        }
        case EditRequest::ReplaceAll: {
            data->DeleteChars(0, data->BufTextLen);
            data->InsertChars(0, st.requestText.c_str());
            data->CursorPos = 0;
            data->SelectionStart = data->SelectionEnd = 0;
            break;
        }
        case EditRequest::None:
            break;
    }
    st.request = EditRequest::None;

    // Vorschlag uebernehmen: das schon Getippte wird durch den Treffer
    // ersetzt. Das muss hier passieren - am std::string vorbei wuerde die
    // Aenderung im naechsten Frame wieder ueberschrieben.
    if (st.acceptRequested) {
        const int from = st.completionStart + 1;
        if (st.completionStart >= 0 && from <= data->BufTextLen && data->CursorPos >= from) {
            data->DeleteChars(from, data->CursorPos - from);
            data->InsertChars(from, st.pickText.c_str());
            st.muted = true;  // nicht sofort wieder aufklappen
            st.mutedWord = st.pickText;
        }
        st.acceptRequested = false;
        st.completionPick = 0;
    }

    st.cursor = data->CursorPos;
    st.selBegin = std::min(data->SelectionStart, data->SelectionEnd);
    st.selEnd = std::max(data->SelectionStart, data->SelectionEnd);

    // Wort vor dem Cursor bestimmen, um "@" (Element) oder "!" (Aktion) zu erkennen
    const std::string previousWord = st.completionWord;
    st.completionStart = -1;
    st.completionSigil = 0;
    st.completionWord.clear();
    int i = data->CursorPos - 1;
    while (i >= 0) {
        const char c = data->Buf[i];
        if (c == '@' || c == '!') {
            st.completionStart = i;
            st.completionSigil = c;
            st.completionWord = std::string(data->Buf + i + 1, data->Buf + data->CursorPos);
            break;
        }
        if (c == '\n' || c == '\t') break;
        if (c == ' ' && st.completionSigil == 0 && i < data->CursorPos - 1) {
            // Leerzeichen beenden nur die Element-Suche, Aktionstitel duerfen welche haben
            bool onlySpaces = true;
            for (int k = i; k < data->CursorPos; ++k) {
                if (data->Buf[k] != ' ') onlySpaces = false;
            }
            if (!onlySpaces && data->Buf[i] == ' ' && i > 0 && data->Buf[i - 1] != '!') break;
        }
        --i;
    }
    if (st.completionSigil == '!' && st.completionWord.rfind("act:", 0) == 0)
        st.completionStart = -1;  // bereits gesetzte Marke nicht erneut anbieten

    // Weitergetippt? Dann darf das Fenster wieder aufgehen, und die
    // Auswahl faengt bei der neuen Trefferliste wieder oben an.
    if (st.completionWord != previousWord) {
        st.completionPick = 0;
        st.pickMoved = false;
        if (st.muted && st.completionWord != st.mutedWord) st.muted = false;
    }
    if (jumped) {
        st.muted = true;
        st.mutedWord = st.completionWord;
    }
    return 0;
}

// ---------------------------------------------------------------- Auftraege
// Alle Wege fuehren hierher: Auftrag merken und die Eingabe zurueck ins
// Textfeld holen, damit der Callback ihn im naechsten Frame ausfuehrt.
void request(ManuscriptState& st, EditRequest kind) {
    st.request = kind;
    st.focusText = true;
    st.restoreCursor = true;
}

void insertAtCursor(ManuscriptState& st, const std::string& text) {
    st.requestText = text;
    // Wer den Namen eben aus dem Menue gewaehlt hat, will nicht direkt danach
    // die Vorschlagsliste fuer genau diesen Namen sehen.
    const size_t sigil = text.find_first_of("@!");
    if (sigil != std::string::npos) {
        st.muted = true;
        st.mutedWord = text.substr(sigil + 1);
    }
    request(st, EditRequest::Insert);
}

// Auswahl umschliessen ("**" ... "**"). Ohne Auswahl landen beide Zeichen am
// Cursor und man schreibt zwischen ihnen weiter.
void wrapSelection(ManuscriptState& st, const std::string& prefix, const std::string& suffix) {
    st.requestText = prefix;
    st.requestSuffix = suffix;
    st.requestBegin = st.selBegin;
    st.requestEnd = std::max(st.selEnd, st.selBegin);
    if (st.requestBegin == st.requestEnd) {
        st.requestBegin = st.cursor;
        st.requestEnd = st.cursor;
    }
    request(st, EditRequest::Wrap);
}

void prefixLine(ManuscriptState& st, const std::string& prefix) {
    st.requestText = prefix;
    request(st, EditRequest::LinePrefix);
}

void selectRange(ManuscriptState& st, size_t begin, size_t end) {
    st.requestBegin = static_cast<int>(begin);
    st.requestEnd = static_cast<int>(end);
    request(st, EditRequest::Select);
}

// Eine Zeitmarke steht immer allein auf ihrer Zeile - sonst liest sie der
// Parser nicht als Marke, sondern als Fliesstext.
void insertTimeMark(Editor& ed, ManuscriptState& st, const std::string& text, long long when) {
    const size_t at = static_cast<size_t>(std::max(0, st.cursor));
    const bool lineStart = at == 0 || (at <= text.size() && text[at - 1] == '\n');
    insertAtCursor(st, (lineStart ? "" : "\n") + ("#" + formatStoryTime(when) + "\n"));
    ed.markManuscript();
}

// Ueberschrift oder Szenenwechsel bekommen immer eine eigene Zeile.
void insertOwnLine(ManuscriptState& st, const std::string& text, const std::string& line) {
    const size_t at = static_cast<size_t>(std::max(0, st.cursor));
    const bool lineStart = at == 0 || (at <= text.size() && text[at - 1] == '\n');
    insertAtCursor(st, (lineStart ? "" : "\n") + line + "\n");
}

// --------------------------------------------------------------- Geometrie
// Wo genau steht ein bestimmtes Byte im Schreibfeld? InputTextMultiline bricht
// Zeilen nicht um, darum genuegt: Zeilennummer mal Zeilenhoehe, und als Spalte
// die Breite des Textes davor. Damit lassen sich die Marken einfaerben und das
// Vorschlagsfenster an den Cursor setzen.
struct TextGeometry {
    bool valid = false;
    ImVec2 origin;  // linke obere Ecke der ersten Zeile, Bildschirmkoordinaten
    float lineHeight = 0.0f;
    ImRect clip;
    // Das Textfeld ist ein eigenes Kindfenster mit eigener Zeichenliste. Wer in
    // die des Elternfensters malt, landet darunter und sieht nichts.
    ImDrawList* drawList = nullptr;
    std::vector<size_t> lineStarts;
};

TextGeometry textGeometry(const char* label, ImGuiID id, const std::string& text) {
    TextGeometry geo;
    ImGuiWindow* parent = ImGui::GetCurrentWindow();
    // InputTextMultiline legt intern ein Kindfenster an; ImGui setzt dessen
    // Namen aus Elternname, Label und Id zusammen.
    char name[512];
    ImFormatString(name, IM_ARRAYSIZE(name), "%s/%s_%08X", parent->Name, label, id);
    ImGuiWindow* inner = ImGui::FindWindowByName(name);
    if (!inner) return geo;

    const ImGuiContext& ctx = *ImGui::GetCurrentContext();
    const float scrollX = ctx.InputTextState.ID == id ? ctx.InputTextState.Scroll.x : 0.0f;
    const ImVec2 pad = ImGui::GetStyle().FramePadding;
    geo.origin = ImVec2(inner->Pos.x + pad.x - scrollX, inner->Pos.y + pad.y - inner->Scroll.y);
    geo.lineHeight = ImGui::GetTextLineHeight();
    geo.clip = inner->InnerClipRect;
    geo.drawList = inner->DrawList;
    geo.lineStarts.push_back(0);
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\n') geo.lineStarts.push_back(i + 1);
    }
    geo.valid = true;
    return geo;
}

size_t lineOfOffset(const TextGeometry& geo, size_t offset) {
    const auto it = std::upper_bound(geo.lineStarts.begin(), geo.lineStarts.end(), offset);
    return static_cast<size_t>(it - geo.lineStarts.begin()) - 1;
}

ImVec2 posOfOffset(const TextGeometry& geo, const std::string& text, size_t offset) {
    offset = std::min(offset, text.size());
    const size_t line = lineOfOffset(geo, offset);
    const size_t start = geo.lineStarts[line];
    const float w = ImGui::CalcTextSize(text.c_str() + start, text.c_str() + offset).x;
    return ImVec2(geo.origin.x + w, geo.origin.y + static_cast<float>(line) * geo.lineHeight);
}

// Rechteck eines Textbereichs innerhalb einer Zeile - fuer Marken und Treffer.
bool spanRect(const TextGeometry& geo, const std::string& text, size_t begin, size_t end,
              ImVec2* topLeft, ImVec2* bottomRight) {
    if (end <= begin) return false;
    const size_t line = lineOfOffset(geo, begin);
    const float y = geo.origin.y + static_cast<float>(line) * geo.lineHeight;
    if (y + geo.lineHeight < geo.clip.Min.y || y > geo.clip.Max.y) return false;
    const size_t start = geo.lineStarts[line];
    const float x0 = geo.origin.x + ImGui::CalcTextSize(text.c_str() + start, text.c_str() + begin).x;
    const float x1 = x0 + ImGui::CalcTextSize(text.c_str() + begin, text.c_str() + end).x;
    *topLeft = ImVec2(x0, y);
    *bottomRight = ImVec2(x1, y + geo.lineHeight);
    return true;
}

// Marken im Schreibmodus sichtbar machen: getoente Flaeche plus Unterstrich in
// der Farbe des Ziels. Unbekannte Verweise werden rot - so faellt ein Tippfehler
// im Namen sofort auf.
void drawMarkHighlights(Editor& ed, const std::string& text, const TextGeometry& geo) {
    if (!geo.valid || !geo.drawList) return;
    ColorScheme& c = theme::colors();
    ImDrawList* dl = geo.drawList;
    dl->PushClipRect(geo.clip.Min, geo.clip.Max, true);

    for (const ManuscriptToken& t : parseManuscript(ed.project, text)) {
        // Hervorgehobener Fliesstext: nur ein zarter Untergrund, damit man
        // sieht, wie weit das Fett bzw. Kursiv reicht. Die Sternchen selbst
        // bleiben aussen vor - so ist zu erkennen, was Auszeichnung ist.
        if (t.kind == ManuscriptToken::Kind::Text) {
            if (!t.bold && !t.italic) continue;
            ImVec2 a, b;
            size_t end = std::min(t.end, text.size());
            while (end > t.begin && (text[end - 1] == '\n' || text[end - 1] == '\r')) --end;
            if (!spanRect(geo, text, t.begin, end, &a, &b)) continue;
            dl->AddRectFilled(a, b, theme::u32(theme::withAlpha(c.textSecondary, 0.13f)), 3.0f);
            continue;
        }
        size_t begin = t.begin;
        size_t end = std::min(t.end, text.size());
        while (end > begin && (text[end - 1] == '\n' || text[end - 1] == '\r')) --end;

        ImVec2 a, b;
        if (!spanRect(geo, text, begin, end, &a, &b)) continue;

        ImVec4 col = c.accentColor;
        switch (t.kind) {
            case ManuscriptToken::Kind::Heading: col = c.accentColor; break;
            case ManuscriptToken::Kind::Break: col = c.textSecondary; break;
            case ManuscriptToken::Kind::Time: col = c.timelineRuler; break;
            case ManuscriptToken::Kind::Action:
                col = t.resolved ? c.warningColor : c.errorColor;
                break;
            default:
                col = t.resolved ? ed.project.elementColor(t.targetId) : c.errorColor;
                break;
        }

        const float thickness = t.kind == ManuscriptToken::Kind::Value ? 2.5f : 1.5f;
        dl->AddRectFilled(ImVec2(a.x - 2.0f, a.y), ImVec2(b.x + 2.0f, b.y),
                          theme::u32(theme::withAlpha(col, 0.16f)), 3.0f);
        dl->AddLine(ImVec2(a.x - 2.0f, b.y - 1.0f), ImVec2(b.x + 2.0f, b.y - 1.0f),
                    theme::u32(col), thickness);
    }
    dl->PopClipRect();
}

// Alle Suchtreffer im Schreibfeld gelb hinterlegen, der angesteuerte kraeftiger.
void drawFindHighlights(ManuscriptState& st, const std::string& text, const TextGeometry& geo,
                        const std::vector<size_t>& hits) {
    if (!geo.valid || !geo.drawList || hits.empty()) return;
    ColorScheme& c = theme::colors();
    ImDrawList* dl = geo.drawList;
    dl->PushClipRect(geo.clip.Min, geo.clip.Max, true);
    for (size_t i = 0; i < hits.size(); ++i) {
        ImVec2 a, b;
        if (!spanRect(geo, text, hits[i], hits[i] + st.findQuery.size(), &a, &b)) continue;
        const bool current = static_cast<int>(i) == st.findHit;
        dl->AddRectFilled(ImVec2(a.x - 1.0f, a.y), ImVec2(b.x + 1.0f, b.y),
                          theme::u32(theme::withAlpha(c.warningColor, current ? 0.55f : 0.25f)),
                          2.0f);
    }
    dl->PopClipRect();
}

// Fliesstext mit eingebetteten, anklickbaren Marken.
void drawRendered(Editor& ed, const std::string& text, bool clickable, bool showTimes) {
    ColorScheme& c = theme::colors();
    const float wrapX = ImGui::GetContentRegionAvail().x;
    const float spaceW = ImGui::CalcTextSize(" ").x;
    float x = 0.0f;
    bool rowHasContent = false;
    ImGui::BeginGroup();

    // Zeile beenden. Steht schon etwas darauf, hat ImGui sie mit dem letzten
    // Element ohnehin beendet - ein zusaetzliches NewLine() wuerde eine leere
    // Zeile erzeugen und den Text doppelt so luftig machen. Nur eine wirklich
    // leere Quellzeile bekommt hier eine eigene.
    auto newLine = [&]() {
        if (!rowHasContent) ImGui::NewLine();
        rowHasContent = false;
        x = 0.0f;
    };
    auto place = [&](float width) {
        if (x > 0.0f && x + width > wrapX) {
            newLine();
        } else if (x > 0.0f) {
            ImGui::SameLine(0.0f, spaceW);
            x += spaceW;
        }
        x += width;
        rowHasContent = true;
    };

    for (const ManuscriptToken& t : parseManuscript(ed.project, text)) {
        // Fett und kursiv sind echte Schriftschnitte - fehlt einer auf dem
        // System, liefert fontFor() die normale Schrift.
        const bool styled = t.bold || t.italic;
        if (styled) ImGui::PushFont(theme::fontFor(t.bold, t.italic), 0.0f);

        switch (t.kind) {
            case ManuscriptToken::Kind::Heading: {
                newLine();
                ImGui::Spacing();
                ImGui::PushStyleColor(ImGuiCol_Text, c.accentColor);
                ImGui::PushFont(theme::fonts().bold, ImGui::GetStyle().FontSizeBase * 1.2f);
                std::string title = trim(t.raw.substr(t.raw.find_first_not_of('#') == std::string::npos
                                                          ? t.raw.size()
                                                          : t.raw.find_first_not_of('#')));
                ImGui::TextUnformatted(title.c_str());
                ImGui::PopFont();
                ImGui::PopStyleColor();
                x = 0.0f;
                rowHasContent = false;  // Ueberschrift, Zeit und Trenner stehen allein
                break;
            }
            case ManuscriptToken::Kind::Break: {
                newLine();
                ImGui::Spacing();
                ImGui::PushStyleColor(ImGuiCol_Text, c.textSecondary);
                const float mid = (wrapX - ImGui::CalcTextSize("* * *").x) * 0.5f;
                if (mid > 0.0f) {
                    ImGui::Dummy(ImVec2(mid, 0.0f));
                    ImGui::SameLine(0.0f, 0.0f);
                }
                ImGui::TextUnformatted("* * *");
                ImGui::PopStyleColor();
                ImGui::Spacing();
                x = 0.0f;
                rowHasContent = false;  // Ueberschrift, Zeit und Trenner stehen allein
                break;
            }
            case ManuscriptToken::Kind::Time: {
                if (!showTimes) break;
                newLine();
                ImGui::PushStyleColor(ImGuiCol_Text, c.textSecondary);
                ImGui::TextUnformatted(("--- " + formatStoryTime(t.time) + " ---").c_str());
                ImGui::PopStyleColor();
                x = 0.0f;
                rowHasContent = false;  // Ueberschrift, Zeit und Trenner stehen allein
                break;
            }
            case ManuscriptToken::Kind::Text: {
                // Wort fuer Wort setzen, damit der Umbruch mit den Marken passt
                std::string word;
                for (size_t i = 0; i <= t.raw.size(); ++i) {
                    const char ch = i < t.raw.size() ? t.raw[i] : ' ';
                    if (ch == '\n') {
                        if (!word.empty()) {
                            place(ImGui::CalcTextSize(word.c_str()).x);
                            ImGui::TextUnformatted(word.c_str());
                            word.clear();
                        }
                        newLine();
                        continue;
                    }
                    if (ch == ' ' || ch == '\t') {
                        if (!word.empty()) {
                            place(ImGui::CalcTextSize(word.c_str()).x);
                            ImGui::TextUnformatted(word.c_str());
                            word.clear();
                        }
                        continue;
                    }
                    word += ch;
                }
                break;
            }
            case ManuscriptToken::Kind::Element:
            case ManuscriptToken::Kind::Value: {
                std::string label;
                ImVec4 col = c.accentColor;
                if (t.kind == ManuscriptToken::Kind::Element) {
                    label = t.resolved ? ed.project.displayName(t.targetId) : t.raw;
                    if (t.resolved) col = ed.project.elementColor(t.targetId);
                } else {
                    const std::string value =
                        t.resolved ? ed.project.valueAt(t.targetId, t.field, t.time) : std::string();
                    if (!t.resolved) {
                        label = t.raw;
                    } else if (value.empty()) {
                        // Feld existiert nicht oder ist leer: Name statt Luecke
                        label = ed.project.displayName(t.targetId);
                    } else {
                        label = value;
                    }
                    col = (t.resolved && !value.empty()) ? c.successColor : c.warningColor;
                }
                if (!t.resolved) col = c.warningColor;

                const float w = ImGui::CalcTextSize(label.c_str()).x + 10.0f;
                place(w);
                ImGui::PushStyleColor(ImGuiCol_Button, theme::withAlpha(col, 0.18f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, theme::withAlpha(col, 0.35f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, theme::withAlpha(col, 0.5f));
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(3, 0));
                if (ImGui::SmallButton((label + "##tok" + std::to_string(t.begin)).c_str()) &&
                    clickable && t.resolved) {
                    ed.select(SelKind::Element, t.targetId);
                    ed.focusElementId = t.targetId;
                    theme::settings().showDetails = true;
                }
                ImGui::PopStyleVar();
                ImGui::PopStyleColor(3);
                if (ImGui::IsItemHovered()) {
                    if (!t.resolved)
                        ImGui::SetTooltip("%s", TR("Nicht gefunden - Name pruefen."));
                    else if (t.kind == ManuscriptToken::Kind::Value &&
                             ed.project.valueAt(t.targetId, t.field, t.time).empty())
                        ImGui::SetTooltip("%s", TR("Feld ist leer - es wird der Name benutzt."));
                    else if (t.kind == ManuscriptToken::Kind::Value)
                        ImGui::SetTooltip("%s.%s  (%s)", ed.project.displayName(t.targetId).c_str(),
                                          t.field.c_str(), formatStoryTime(t.time).c_str());
                    else
                        ImGui::SetTooltip("%s", ed.project.elementPath(t.targetId).c_str());
                }
                break;
            }
            case ManuscriptToken::Kind::Action: {
                if (!showTimes) break;  // Marken ausgeblendet
                const Action* a = ed.project.action(t.targetId);
                std::string label = a ? std::string("-> ") + a->title : t.raw;
                const float w = ImGui::CalcTextSize(label.c_str()).x + 10.0f;
                place(w);
                ImGui::PushStyleColor(ImGuiCol_Button, theme::withAlpha(c.warningColor, 0.18f));
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(3, 0));
                if (ImGui::SmallButton((label + "##act" + std::to_string(t.begin)).c_str()) &&
                    clickable && a) {
                    ed.select(SelKind::Action, a->id);
                    ed.focusActionId = a->id;
                    ed.focusTimeline = true;
                    theme::settings().showDetails = true;
                }
                ImGui::PopStyleVar();
                ImGui::PopStyleColor();
                break;
            }
        }
        if (styled) ImGui::PopFont();
    }
    ImGui::EndGroup();
}

// Aus dem Absatz um den Cursor eine Aktion machen.
void actionFromParagraph(Editor& ed, ManuscriptState& st) {
    std::string& text = ed.project.manuscript;
    size_t begin = 0, end = 0;
    paragraphAt(text, static_cast<size_t>(std::max(0, st.cursor)), &begin, &end);
    const std::string paragraph = text.substr(begin, end - begin);
    if (trim(paragraph).empty()) {
        ed.setStatus(TR("Der Absatz am Cursor ist leer."), true);
        return;
    }

    const long long time = timeAtOffset(ed.project, text, begin);
    const std::vector<std::string> mentioned = mentionedElements(ed.project, paragraph);

    // Titel: erster Satz des Absatzes, gerendert
    std::string title = trim(renderManuscript(ed.project, paragraph));
    for (size_t i = 0; i < title.size(); ++i) {
        if (title[i] == '.' || title[i] == '!' || title[i] == '?' || title[i] == '\n') {
            title = title.substr(0, i);
            break;
        }
    }
    if (title.size() > 70) title = title.substr(0, 70);

    ed.pushUndo(TR("Aktion aus Absatz"));
    Action& a = ed.project.addAction(title, time);
    a.description = trim(renderManuscript(ed.project, paragraph));
    a.elementIds = mentioned;
    if (!ed.project.actionTypes.empty()) a.type = ed.project.actionTypes.front();

    // Marke ans Ende des Absatzes setzen
    const std::string marker = " !act:" + a.id;
    text.insert(end, marker);
    ed.markActions();
    ed.markManuscript();
    ed.select(SelKind::Action, a.id);
    ed.setStatus(TR("Aktion angelegt: ") + title);
}

// ------------------------------------------------------------------ Menues
// Elementliste mit Suchfeld - einmal fuer "@Name" und einmal fuer "@Name.feld".
// Im zweiten Fall klappt jedes Element seine Felder als Untermenue auf, und
// daneben steht gleich der Wert, der zu diesem Zeitpunkt gilt.
void elementPickerMenu(Editor& ed, ManuscriptState& st, bool withField, long long timeHere) {
    ImGui::SetNextItemWidth(220.0f);
    ImGui::InputTextWithHint("##elemsearch", TR("Suchen..."), &st.elementSearch);
    ImGui::Separator();

    int shown = 0;
    for (const Element& el : ed.project.elements) {
        if (!st.elementSearch.empty() && !iequalsContains(el.name, st.elementSearch) &&
            !iequalsContains(ed.project.elementPath(el.id), st.elementSearch))
            continue;
        ImGui::PushID(el.id.c_str());
        ui::colorDot(ed.project.elementColor(el.id));

        if (withField) {
            if (ImGui::BeginMenu(el.name.c_str())) {
                int fields = 0;
                for (const std::string& key : el.fieldOrder) {
                    const std::string value = ed.project.valueAt(el.id, key, timeHere);
                    const std::string label =
                        key + (value.empty() ? std::string("   (leer)")
                                             : "   " + ui::ellipsis(value, 24));
                    if (ImGui::MenuItem(label.c_str())) {
                        insertAtCursor(st, "@" + el.name + "." + key);
                        ed.markManuscript();
                    }
                    ++fields;
                }
                if (fields == 0) ui::textSecondary(TR("Dieses Element hat keine Felder."));
                ImGui::EndMenu();
            }
        } else if (ImGui::MenuItem(el.name.c_str())) {
            insertAtCursor(st, "@" + el.name);
            ed.markManuscript();
        }
        ui::tooltip(ed.project.elementPath(el.id).c_str());
        ImGui::PopID();
        if (++shown >= 14) break;
    }
    if (shown == 0) ui::textSecondary(TR("Nichts gefunden."));
}

void actionPickerMenu(Editor& ed, ManuscriptState& st, long long timeHere) {
    ImGui::SetNextItemWidth(260.0f);
    ImGui::InputTextWithHint("##actsearch", TR("Aktion suchen..."), &st.actionSearch);
    ImGui::Separator();
    int shown = 0;
    for (const Action* a : ed.project.sortedActions()) {
        if (!st.actionSearch.empty() && !iequalsContains(a->title, st.actionSearch)) continue;
        ImGui::PushID(a->id.c_str());
        ui::textSecondary(formatStoryTime(ed.project.resolveActionTime(*a)).c_str());
        ImGui::SameLine();
        if (ImGui::MenuItem(a->title.c_str())) {
            insertAtCursor(st, " !act:" + a->id);
            ed.markManuscript();
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopID();
        if (++shown >= 12) break;
    }
    if (shown == 0) ui::textSecondary(TR("Keine passende Aktion."));
    ImGui::Separator();
    if (ImGui::MenuItem(TR("+ Neue Aktion hier"))) {
        ed.pushUndo(TR("Aktion erstellt"));
        const std::string title =
            st.actionSearch.empty() ? std::string(TR("Neue Aktion")) : st.actionSearch;
        Action& a = ed.project.addAction(title, timeHere);
        if (!ed.project.actionTypes.empty()) a.type = ed.project.actionTypes.front();
        size_t begin = 0, end = 0;
        paragraphAt(ed.project.manuscript, static_cast<size_t>(std::max(0, st.cursor)), &begin,
                    &end);
        a.elementIds =
            mentionedElements(ed.project, ed.project.manuscript.substr(begin, end - begin));
        insertAtCursor(st, " !act:" + a.id);
        ed.markActions();
        ed.markManuscript();
        ed.select(SelKind::Action, a.id);
        ImGui::CloseCurrentPopup();
    }
    ui::tooltip(TR("Legt eine Aktion zum Zeitpunkt dieser Stelle an und verknuepft sie."));
}

void timeMenu(Editor& ed, ManuscriptState& st, const std::string& text, long long timeHere) {
    ImGui::TextWrapped("%s", TR("Die Zeit laeuft nicht von allein weiter: sie bleibt stehen, "
                                "bis du sie weiterstellst. Ab der Marke gilt der neue "
                                "Zeitpunkt fuer alles, was danach im Text kommt."));
    ImGui::Spacing();
    ui::textSecondary((TR("Hier gilt gerade: ") + formatStoryTime(timeHere)).c_str());
    ImGui::Separator();

    ui::textSecondary(TR("Weiter um:"));
    struct Quick {
        const char* label;
        long long offset;
    };
    const Quick quick[] = {
        {"+ 1 Stunde", kMinutesPerHour},   {"+ 6 Stunden", 6 * kMinutesPerHour},
        {"+ 1 Tag", kMinutesPerDay},       {"+ 3 Tage", 3 * kMinutesPerDay},
        {"+ 1 Woche", 7 * kMinutesPerDay}, {"+ 1 Monat", 30 * kMinutesPerDay},
    };
    for (const Quick& q : quick) {
        if (ImGui::Button(TR(q.label), ImVec2(110, 0))) {
            insertTimeMark(ed, st, text, timeHere + q.offset);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        // Was dabei herauskommt, gleich danebenschreiben - dann muss niemand
        // die Zeitrechnung im Kopf machen.
        ui::textSecondary(formatStoryTime(timeHere + q.offset).c_str());
    }

    ImGui::Separator();
    ui::textSecondary(TR("Oder fester Zeitpunkt:"));
    ImGui::SetNextItemWidth(180.0f);
    const bool entered =
        ImGui::InputText("##timedraft", &st.timeDraft, ImGuiInputTextFlags_EnterReturnsTrue);
    long long parsed = 0;
    const bool ok = parseStoryTime(st.timeDraft, &parsed);
    ImGui::SameLine();
    if (!ok) ImGui::BeginDisabled();
    if ((ImGui::Button(TR("Einfuegen")) || (entered && ok)) && ok) {
        insertTimeMark(ed, st, text, parsed);
        ImGui::CloseCurrentPopup();
    }
    if (!ok) ImGui::EndDisabled();
    ui::textSecondary(TR("z.B. \"Tag 5, 14:00\" oder \"Jahr 2, Monat 3, Tag 15\""));
}

// ------------------------------------------------------------------- Hilfe
void drawHelpWindow(ManuscriptState& st) {
    if (!st.showHelp) return;
    ImGui::SetNextWindowSize(ImVec2(470, 0), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(TWIN("Wie das Schreiben hier funktioniert", "manuscript_help"), &st.showHelp,
                     ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("%s",
                           TR("Schreib einfach los wie in jedem Textprogramm. Alles Weitere "
                              "setzt die Leiste oben fuer dich ein - du musst dir keine "
                              "Sonderzeichen merken."));
        ImGui::Spacing();
        ImGui::Separator();

        struct Row {
            const char* what;
            const char* how;
        };
        const Row rows[] = {
            {"Eine Figur, ein Ort, ein Ding nennen",
             "Einfuegen > Element - oder einfach @ tippen und weiterschreiben."},
            {"Einen Wert einsetzen (Alter, Titel, Zustand)",
             "Einfuegen > Wert eines Elements. Im Text steht dann immer der Wert, "
             "der zu diesem Zeitpunkt der Geschichte gilt."},
            {"Zeit vergehen lassen",
             "Einfuegen > Zeitpunkt. Die Zeit bleibt stehen, bis du sie weiterstellst."},
            {"Eine Stelle als Ereignis merken",
             "Einfuegen > Aktion aus diesem Absatz - Beteiligte und Zeitpunkt kommen aus dem Text."},
            {"Kapitel und Szenen gliedern",
             "Format > Kapitel bzw. Szene. Beides erscheint links in der Gliederung."},
            {"Hervorheben", "Format > Fett / Kursiv, oder Strg+B und Strg+I."},
            {"Etwas wiederfinden", "Suchen (Strg+F) - findet auch Namen und Marken."},
        };
        for (const Row& row : rows) {
            ImGui::Spacing();
            ImGui::TextUnformatted(TR(row.what));
            ImGui::PushStyleColor(ImGuiCol_Text, theme::colors().textSecondary);
            ImGui::TextWrapped("%s", TR(row.how));
            ImGui::PopStyleColor();
        }
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextWrapped("%s", TR("Die Marken sind nur im Schreibmodus zu sehen. Im Lesemodus "
                                    "und in der Word-Datei steht der fertige Text."));
    }
    ImGui::End();
}

// ------------------------------------------------------------------ Suchen
void drawFindBar(Editor& ed, ManuscriptState& st, const std::string& text,
                 const std::vector<size_t>& hits) {
    ColorScheme& c = theme::colors();
    ImGui::PushStyleColor(ImGuiCol_ChildBg, theme::mix(c.panelBackground, c.backgroundColor, 0.4f));
    ImGui::BeginChild("findbar", ImVec2(0, ImGui::GetFrameHeight() + ImGui::GetStyle().WindowPadding.y * 2.0f),
                      ImGuiChildFlags_Borders,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    if (st.findFocus) {
        ImGui::SetKeyboardFocusHere();
        st.findFocus = false;
    }
    ImGui::SetNextItemWidth(200.0f);
    ImGui::InputTextWithHint("##find", TR("Im Text suchen..."), &st.findQuery);
    const bool queryActive = ImGui::IsItemActive();

    auto goTo = [&](int index) {
        if (hits.empty()) return;
        const int count = static_cast<int>(hits.size());
        st.findHit = ((index % count) + count) % count;
        selectRange(st, hits[static_cast<size_t>(st.findHit)],
                    hits[static_cast<size_t>(st.findHit)] + st.findQuery.size());
    };

    ImGui::SameLine();
    if (ImGui::ArrowButton("##findprev", ImGuiDir_Up)) goTo(st.findHit - 1);
    ui::tooltip(TR("Vorheriger Treffer (Shift+F3)"));
    ImGui::SameLine();
    if (ImGui::ArrowButton("##findnext", ImGuiDir_Down)) goTo(st.findHit + 1);
    ui::tooltip(TR("Naechster Treffer (F3)"));

    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, hits.empty() && !st.findQuery.empty() ? c.warningColor
                                                                              : c.textSecondary);
    if (st.findQuery.empty())
        ImGui::TextUnformatted("-");
    else if (hits.empty())
        ImGui::TextUnformatted(TR("kein Treffer"));
    else
        ImGui::Text("%d / %zu", st.findHit + 1, hits.size());
    ImGui::PopStyleColor();

    ImGui::SameLine();
    ImGui::Checkbox(TR("Gross/klein"), &st.findCase);
    ui::tooltip(TR("Gross- und Kleinschreibung beachten"));

    ImGui::SameLine();
    ImGui::SetNextItemWidth(200.0f);
    ImGui::InputTextWithHint("##replace", TR("Ersetzen durch..."), &st.replaceQuery);

    ImGui::SameLine();
    const bool canReplace = !hits.empty();
    if (!canReplace) ImGui::BeginDisabled();
    if (ImGui::Button(TR("Ersetzen"))) {
        const size_t at = hits[static_cast<size_t>(
            std::min<int>(st.findHit, static_cast<int>(hits.size()) - 1))];
        ed.pushUndo(TR("Ersetzen"));
        st.requestBegin = static_cast<int>(at);
        st.requestEnd = static_cast<int>(at + st.findQuery.size());
        st.requestText = st.replaceQuery;
        request(st, EditRequest::Replace);
        ed.markManuscript();
    }
    ImGui::SameLine();
    if (ImGui::Button(TR("Alle ersetzen"))) {
        std::string out;
        size_t last = 0;
        for (size_t at : hits) {
            out += text.substr(last, at - last);
            out += st.replaceQuery;
            last = at + st.findQuery.size();
        }
        out += text.substr(last);
        ed.pushUndo(TR("Alle ersetzen"));
        st.requestText = out;
        request(st, EditRequest::ReplaceAll);
        ed.markManuscript();
        ed.setStatus(std::to_string(hits.size()) + TR(" Stellen ersetzt."));
        st.findHit = 0;
    }
    if (!canReplace) ImGui::EndDisabled();

    ImGui::SameLine();
    if (ImGui::Button(TR("Schliessen"))) st.showFind = false;

    // F3 und Enter im Suchfeld springen weiter.
    const bool enter = queryActive && (ImGui::IsKeyPressed(ImGuiKey_Enter, false) ||
                                       ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false));
    if (ImGui::IsKeyPressed(ImGuiKey_F3, false) || enter)
        goTo(ImGui::GetIO().KeyShift ? st.findHit - 1 : st.findHit + 1);

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

}  // namespace

void drawManuscriptWindow(Editor& ed, bool* open) {
    ImGui::SetNextWindowSize(ImVec2(900, 620), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(TWIN("Manuskript", "manuscript"), open)) {
        ImGui::End();
        return;
    }
    if (!ed.project.loaded) {
        ui::textSecondary(TR("Kein Projekt geoeffnet."));
        ImGui::End();
        return;
    }

    ManuscriptState& st = state();
    ColorScheme& c = theme::colors();
    std::string& text = ed.project.manuscript;
    const bool windowFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    const long long timeHere =
        timeAtOffset(ed.project, text, static_cast<size_t>(std::max(0, st.cursor)));

    // -------------------------------------------------------------- Leiste
    // Gruppe 1: worauf schaue ich gerade?
    if (ImGui::RadioButton(TR("Schreiben"), !st.readMode)) st.readMode = false;
    ui::tooltip(TR("Text bearbeiten - Marken sind sichtbar."));
    ImGui::SameLine();
    if (ImGui::RadioButton(TR("Lesen"), st.readMode)) st.readMode = true;
    ui::tooltip(TR("So liest sich die Geschichte fertig - alle Werte sind eingesetzt."));

    ImGui::SameLine();
    ui::verticalSeparator();

    // Gruppe 2: was kommt in den Text?
    const bool writing = !st.readMode;
    if (!writing) ImGui::BeginDisabled();
    if (ImGui::Button(TR("Einfuegen"))) ImGui::OpenPopup("ms_insert");
    ui::tooltip(TR("Elemente, Werte, Aktionen und Zeitpunkte in den Text setzen - ohne "
                   "Sonderzeichen tippen zu muessen."));
    if (ImGui::BeginPopup("ms_insert")) {
        if (ImGui::BeginMenu(TR("Element (Figur, Ort, Ding)"))) {
            elementPickerMenu(ed, st, false, timeHere);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(TR("Wert eines Elements"))) {
            elementPickerMenu(ed, st, true, timeHere);
            ImGui::EndMenu();
        }
        ImGui::Separator();
        if (ImGui::BeginMenu(TR("Aktion verknuepfen"))) {
            actionPickerMenu(ed, st, timeHere);
            ImGui::EndMenu();
        }
        if (ImGui::MenuItem(TR("Aktion aus diesem Absatz"))) actionFromParagraph(ed, st);
        ui::tooltip(TR("Macht aus dem Absatz am Cursor eine Aktion - Beteiligte und Zeitpunkt "
                       "kommen aus dem Text."));
        ImGui::Separator();
        if (ImGui::BeginMenu(TR("Zeitpunkt weiterstellen"))) {
            timeMenu(ed, st, text, timeHere);
            ImGui::EndMenu();
        }
        ImGui::EndPopup();
    }

    ImGui::SameLine();
    if (ImGui::Button(TR("Format"))) ImGui::OpenPopup("ms_format");
    ui::tooltip(TR("Ueberschriften und Hervorhebungen."));
    if (ImGui::BeginPopup("ms_format")) {
        if (ImGui::MenuItem(TR("Kapitel"), "")) {
            insertOwnLine(st, text, "## " + std::string(TR("Neues Kapitel")));
            ed.markManuscript();
        }
        ui::tooltip(TR("Erscheint links in der Gliederung und im Word-Export als Ueberschrift."));
        if (ImGui::MenuItem(TR("Szene"), "")) {
            insertOwnLine(st, text, "### " + std::string(TR("Neue Szene")));
            ed.markManuscript();
        }
        if (ImGui::MenuItem(TR("Szenenwechsel"), "")) {
            insertOwnLine(st, text, "---");
            ed.markManuscript();
        }
        ui::tooltip(TR("Eine Trennung mitten im Kapitel - im Export als * * * zentriert."));
        ImGui::Separator();
        if (ImGui::MenuItem(TR("Fett"), TR("Strg+B"))) {
            wrapSelection(st, "**", "**");
            ed.markManuscript();
        }
        if (ImGui::MenuItem(TR("Kursiv"), TR("Strg+I"))) {
            wrapSelection(st, "*", "*");
            ed.markManuscript();
        }
        ui::tooltip(TR("Markierten Text hervorheben - ohne Auswahl schreibst du zwischen den "
                       "Zeichen weiter."));
        ImGui::EndPopup();
    }
    if (!writing) ImGui::EndDisabled();

    ImGui::SameLine();
    const bool findActive = st.showFind;
    if (findActive) ImGui::PushStyleColor(ImGuiCol_Button, theme::accentFill());
    if (ImGui::Button(TR("Suchen"))) {
        st.showFind = !st.showFind;
        st.findFocus = st.showFind;
    }
    if (findActive) ImGui::PopStyleColor();
    ui::tooltip(TR("Stellen im Text finden und ersetzen (Strg+F)."));

    ImGui::SameLine();
    ui::verticalSeparator();

    // Gruppe 3: was sehe ich zusaetzlich?
    if (ImGui::Button(TR("Ansicht"))) ImGui::OpenPopup("ms_view");
    if (ImGui::BeginPopup("ms_view")) {
        ImGui::MenuItem(TR("Gliederung"), nullptr, &st.showOutline);
        ImGui::MenuItem(TR("Marken zeigen"), nullptr, &st.showMarks);
        ui::tooltip(TR("Nur zur Orientierung beim Lesen - im Export steht sie nie."));
        ImGui::Separator();
        if (ImGui::MenuItem(TR("Als Word speichern..."))) ed.exportManuscriptToWord();
        ui::tooltip(TR("Schreibt die fertige Geschichte: Werte sind fest eingesetzt, "
                       "Zeit- und Aktionsmarken stehen nicht darin."));
        ImGui::EndPopup();
    }

    ImGui::SameLine();
    if (ImGui::Button("?")) st.showHelp = true;
    ui::tooltip(TR("Kurz erklaert, wie dieses Fenster funktioniert."));

    // Gruppe 4: Stand der Dinge - rechtsbuendig, damit die Leiste ruhig bleibt.
    {
        const std::string timeLabel = TR("Zeit: ") + formatStoryTime(timeHere) + "###mstime";
        const std::string words = std::to_string(countWords(text)) + " " + TR("Woerter");
        const bool unsaved = ed.hasUnsavedChanges();
        const std::string saveLabel = unsaved ? TR("Speichern") : TR("Gespeichert");
        const ImGuiStyle& style = ImGui::GetStyle();
        // "###mstime" gehoert zur Id, nicht zur Beschriftung - sonst waere die
        // gemessene Breite zu gross.
        const float timeWidth =
            ImGui::CalcTextSize(timeLabel.c_str(), timeLabel.c_str() + timeLabel.find("###")).x;
        const float widthNeeded = timeWidth + style.FramePadding.x * 2.0f +
                                  ImGui::CalcTextSize(words.c_str()).x +
                                  ImGui::CalcTextSize(saveLabel.c_str()).x +
                                  style.FramePadding.x * 2.0f + style.ItemSpacing.x * 3.0f + 16.0f;
        ImGui::SameLine();
        const float slack = ImGui::GetContentRegionAvail().x - widthNeeded;
        if (slack > 0.0f) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + slack);

        // Der Knopf ist zugleich die Anzeige: man sieht immer, welcher Zeitpunkt
        // an der Cursorstelle gilt. Ohne das merkt niemand, dass es ihn gibt.
        ImGui::PushStyleColor(ImGuiCol_Text, c.accentColor);
        const bool timeClicked = ImGui::Button(timeLabel.c_str());
        ImGui::PopStyleColor();
        if (timeClicked) {
            st.timeDraft = formatStoryTime(timeHere);
            ImGui::OpenPopup("ms_time");
        }
        ui::tooltip(TR("Der Zeitpunkt, an dem die Geschichte an dieser Stelle steht. Klicken, um "
                       "die Zeit ab hier weiterzustellen."));
        if (ImGui::BeginPopup("ms_time")) {
            timeMenu(ed, st, text, timeHere);
            ImGui::EndPopup();
        }

        ImGui::SameLine();
        ui::textSecondary(words.c_str());

        ImGui::SameLine();
        ui::colorDot(unsaved ? c.warningColor : c.successColor);
        if (unsaved) {
            if (ImGui::SmallButton(saveLabel.c_str())) ed.saveEverything();
            ui::tooltip(theme::settings().autosave
                            ? TR("Wird gleich von selbst geschrieben - Klick speichert sofort.")
                            : TR("Automatisches Speichern ist aus. Klicken oder Strg+S."));
        } else {
            ui::textSecondary(saveLabel.c_str());
            ui::tooltip(TR("Alles steht im Vault."));
        }
    }

    ImGui::Separator();

    // -------------------------------------------------------------- Suchen
    const std::vector<size_t> hits =
        st.showFind ? findAll(text, st.findQuery, st.findCase) : std::vector<size_t>();
    if (st.showFind) {
        if (st.findHit >= static_cast<int>(hits.size())) st.findHit = 0;
        drawFindBar(ed, st, text, hits);
    }

    // Tastenkuerzel des Fensters
    if (windowFocused) {
        const ImGuiIO& io = ImGui::GetIO();
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_F, false)) {
            st.showFind = true;
            st.findFocus = true;
        }
        if (io.KeyCtrl && !st.readMode && ImGui::IsKeyPressed(ImGuiKey_B, false)) {
            wrapSelection(st, "**", "**");
            ed.markManuscript();
        }
        if (io.KeyCtrl && !st.readMode && ImGui::IsKeyPressed(ImGuiKey_I, false)) {
            wrapSelection(st, "*", "*");
            ed.markManuscript();
        }
    }

    // ------------------------------------------------------------- outline
    if (st.showOutline) {
        ImGui::BeginChild("outline", ImVec2(200, 0), ImGuiChildFlags_Borders);
        ImGui::TextUnformatted(TR("Gliederung"));
        ui::tooltip(TR("Klick springt an die Stelle im Text."));
        ImGui::Separator();
        bool any = false;
        for (const ManuscriptToken& t : parseManuscript(ed.project, text)) {
            if (t.kind == ManuscriptToken::Kind::Heading) {
                any = true;
                std::string title = t.raw;
                const size_t hashes = title.find_first_not_of('#');
                const int level = hashes == std::string::npos ? 1 : static_cast<int>(hashes);
                const size_t firstText = title.find_first_not_of("# ");
                title = firstText == std::string::npos ? title : title.substr(firstText);
                ImGui::PushID(static_cast<int>(t.begin));
                if (level >= 3) ImGui::Indent(12.0f);
                if (ImGui::Selectable(title.c_str())) selectRange(st, t.begin, t.begin);
                if (level >= 3) ImGui::Unindent(12.0f);
                ImGui::PopID();
            } else if (t.kind == ManuscriptToken::Kind::Time) {
                any = true;
                ImGui::PushID(static_cast<int>(t.begin));
                ImGui::PushStyleColor(ImGuiCol_Text, c.textSecondary);
                if (ImGui::Selectable(("- " + formatStoryTime(t.time)).c_str()))
                    selectRange(st, t.begin, t.begin);
                ImGui::PopStyleColor();
                ImGui::PopID();
            }
        }
        if (!any)
            ui::textSecondary(TR("Kapitel und Zeitpunkte aus dem Menue \"Format\" bzw. "
                                 "\"Einfuegen\" erscheinen hier."));
        ImGui::EndChild();
        ImGui::SameLine();
    }

    // -------------------------------------------------------------- pages
    ImGui::BeginChild("page", ImVec2(0, 0), ImGuiChildFlags_Borders);
    if (st.readMode) {
        drawRendered(ed, text, true, st.showMarks);
    } else {
        // Ein leeres Manuskript sagt von sich aus nichts. Der Hinweis steht nur
        // hier - sobald das erste Wort im Text steht, verschwindet er.
        if (trim(text).empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, c.textSecondary);
            ImGui::TextWrapped("%s", TR("Einfach losschreiben - wie in jedem Textprogramm. Figuren, "
                                        "Werte, Zeitpunkte und Ueberschriften setzt du oben ueber "
                                        "\"Einfuegen\" und \"Format\" ein. Das \"?\" erklaert den Rest."));
            ImGui::PopStyleColor();
            ImGui::Separator();
        }

        ImGui::PushStyleColor(ImGuiCol_FrameBg, theme::mix(c.panelBackground, c.backgroundColor, 0.2f));

        // Solange Vorschlaege offen sind, gehoeren Enter/Tab/Pfeile/Esc uns.
        // Ohne das Beanspruchen wuerde das Textfeld einen Zeilenumbruch bzw.
        // einen Tabulator einfuegen, statt den Vorschlag zu uebernehmen.
        if (st.completionOpen) {
            const ImGuiID owner = ImGui::GetID("##completion_keys");
            // Enter greift nur, wenn wirklich ausgewaehlt wurde - sonst bliebe
            // hinter einem Satzende wie "@Castle." kein Weg zum Absatz.
            const bool enterAccepts = st.pickFiltered || st.pickMoved;
            const bool byEnter = ImGui::IsKeyPressed(ImGuiKey_Enter, false) ||
                                 ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false);
            const bool accept = ImGui::IsKeyPressed(ImGuiKey_Tab, false) ||
                                (byEnter && enterAccepts);
            if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, true)) {
                ++st.completionPick;
                st.pickMoved = true;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, true)) {
                --st.completionPick;
                st.pickMoved = true;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
                st.muted = true;
                st.mutedWord = st.completionWord;
            }
            if (accept && !st.pickText.empty()) st.acceptRequested = true;
            for (ImGuiKey key : {ImGuiKey_Tab, ImGuiKey_Escape, ImGuiKey_UpArrow,
                                 ImGuiKey_DownArrow})
                ImGui::SetKeyOwner(key, owner);
            if (enterAccepts) {
                ImGui::SetKeyOwner(ImGuiKey_Enter, owner);
                ImGui::SetKeyOwner(ImGuiKey_KeypadEnter, owner);
            }
        }

        // Ein Auftrag aus der Leiste wartet: Eingabe zurueck ins Textfeld, der
        // Callback fuehrt ihn dann im selben Frame aus.
        if (st.focusText) {
            ImGui::SetKeyboardFocusHere();
            st.focusText = false;
        }

        ImGui::InputTextMultiline("##text", &text, ImVec2(-1, -1),
                                  ImGuiInputTextFlags_CallbackAlways, editCallback, &st);
        const ImGuiID inputId = ImGui::GetItemID();
        const bool editing = ImGui::IsItemActive();
        if (ImGui::IsItemEdited()) ed.markManuscript();
        ImGui::PopStyleColor();

        // Marken sichtbar machen - sonst sieht der geschriebene Text aus wie
        // Fliesstext und man erkennt keine Verweise.
        const TextGeometry geo = textGeometry("##text", inputId, text);
        drawMarkHighlights(ed, text, geo);
        if (st.showFind) drawFindHighlights(st, text, geo, hits);

        // ------------------------------------------------ Autovervollstaendigung
        st.completionOpen = false;
        if (editing && st.completionStart >= 0 && !st.muted) {
            // Nach einem Punkt werden die Felder des Elements angeboten:
            // "@Robert." -> spitzname, alter, ...
            const size_t dot = st.completionWord.find('.');
            std::vector<std::string> matches;   // was eingefuegt wird
            std::vector<std::string> labels;    // was angezeigt wird
            std::vector<ImVec4> colors;
            bool fieldMode = false;
            const bool actionMode = st.completionSigil == '!';
            // Wurde etwas getippt, das die Liste einengt? Nur dann darf Enter
            // uebernehmen statt einen Absatz zu machen.
            bool filtered = !st.completionWord.empty();

            if (actionMode) {
                for (const Action* a : ed.project.sortedActions()) {
                    if (!st.completionWord.empty() &&
                        !iequalsContains(a->title, st.completionWord))
                        continue;
                    matches.push_back("act:" + a->id);
                    labels.push_back(formatStoryTime(ed.project.resolveActionTime(*a)) + "  " +
                                     a->title);
                    colors.push_back(theme::colors().warningColor);
                    if (matches.size() >= 8) break;
                }
            } else if (dot != std::string::npos) {
                const std::string elementPart = st.completionWord.substr(0, dot);
                const std::string fieldPart = st.completionWord.substr(dot + 1);
                const Element* owner = nullptr;
                for (const Element& el : ed.project.elements) {
                    if (el.name == elementPart || ed.project.elementPath(el.id) == elementPart)
                        owner = &el;
                }
                if (owner) {
                    fieldMode = true;
                    filtered = !fieldPart.empty();
                    for (const std::string& key : owner->fieldOrder) {
                        if (!fieldPart.empty() && !iequalsContains(key, fieldPart)) continue;
                        const std::string value = ed.project.valueAt(owner->id, key, timeHere);
                        matches.push_back(elementPart + "." + key);
                        labels.push_back(key + (value.empty() ? std::string(" (leer)")
                                                             : "  -  " + ui::ellipsis(value, 24)));
                        colors.push_back(value.empty() ? c.warningColor : c.successColor);
                        if (matches.size() >= 8) break;
                    }
                }
            }
            if (!fieldMode && !actionMode) {
                for (const Element& el : ed.project.elements) {
                    if (st.completionWord.empty() || iequalsContains(el.name, st.completionWord) ||
                        iequalsContains(ed.project.elementPath(el.id), st.completionWord)) {
                        matches.push_back(el.name);
                        labels.push_back(el.name);
                        colors.push_back(ed.project.elementColor(el.id));
                    }
                    if (matches.size() >= 8) break;
                }
            }
            if (!matches.empty()) {
                const int count = static_cast<int>(matches.size());
                if (st.completionPick < 0) st.completionPick = count - 1;
                if (st.completionPick >= count) st.completionPick = 0;
                st.pickText = matches[static_cast<size_t>(st.completionPick)];
                st.pickFiltered = filtered;
                st.completionOpen = true;

                // direkt unter die Marke setzen, nicht quer ueber den Text
                ImVec2 at(ImGui::GetItemRectMin().x + 40.0f, ImGui::GetItemRectMin().y + 40.0f);
                if (geo.valid) {
                    const ImVec2 caret =
                        posOfOffset(geo, text, static_cast<size_t>(st.completionStart));
                    at = ImVec2(std::min(caret.x, geo.clip.Max.x - 240.0f),
                                caret.y + geo.lineHeight + 3.0f);
                    at.x = std::max(at.x, geo.clip.Min.x);
                }
                ImGui::SetNextWindowPos(at, ImGuiCond_Always);
                ImGui::SetNextWindowBgAlpha(0.95f);
                ImGui::Begin("##completion", nullptr,
                             ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize |
                                 ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
                                 ImGuiWindowFlags_NoMove);
                ui::textSecondary(actionMode  ? TR("Aktion waehlen - Enter uebernimmt")
                                  : fieldMode ? TR("Feld waehlen - Enter uebernimmt")
                                              : TR("Enter uebernimmt, Pfeile waehlen, Esc schliesst"));
                for (size_t i = 0; i < matches.size(); ++i) {
                    ui::colorDot(colors[i]);
                    const bool picked = static_cast<int>(i) == st.completionPick;
                    ImGui::TextUnformatted(labels[i].c_str());
                    if (picked) {
                        ImGui::SameLine();
                        ImGui::PushStyleColor(ImGuiCol_Text, c.accentColor);
                        ImGui::TextUnformatted("<");
                        ImGui::PopStyleColor();
                    }
                }
                ImGui::End();
                // Enter, Tab, Pfeile und Esc werden vor dem Textfeld
                // ausgewertet - hier gibt es nichts mehr zu tun.
            }
        }
        if (!st.completionOpen) {
            st.pickText.clear();
            st.pickFiltered = false;
            st.pickMoved = false;
        }
    }
    ImGui::EndChild();

    drawHelpWindow(st);
    ImGui::End();
}

}  // namespace se
