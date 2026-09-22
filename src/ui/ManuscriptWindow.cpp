// Manuskript-Fenster: hier wird die Geschichte geschrieben. Die Struktur
// entsteht nebenbei aus Marken im Text (siehe core/Manuscript.h).
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

struct ManuscriptState {
    bool readMode = false;
    bool showOutline = true;
    bool showMarks = true;       // Zeit- und Aktionsmarken: nur Ansicht, nie im Text
    std::string actionSearch;
    std::string timeDraft;
    int cursor = 0;              // letzte bekannte Cursorposition
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
    bool insertRequested = false;
    std::string insertText;
    float scrollToLine = -1.0f;
};

ManuscriptState& state() {
    static ManuscriptState s;
    return s;
}

// Callback: merkt sich die Cursorposition und fuegt Text ein, wenn gewuenscht.
int editCallback(ImGuiInputTextCallbackData* data) {
    ManuscriptState& st = *static_cast<ManuscriptState*>(data->UserData);
    if (data->EventFlag == ImGuiInputTextFlags_CallbackAlways) {
        st.cursor = data->CursorPos;
        if (st.insertRequested) {
            data->InsertChars(data->CursorPos, st.insertText.c_str());
            st.insertRequested = false;
            st.insertText.clear();
        }
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
    }
    return 0;
}

void insertAtCursor(ManuscriptState& st, const std::string& text) {
    st.insertRequested = true;
    st.insertText = text;
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

// Marken im Schreibmodus sichtbar machen: getoente Flaeche plus Unterstrich in
// der Farbe des Ziels. Unbekannte Verweise werden rot - so faellt ein Tippfehler
// im Namen sofort auf.
void drawMarkHighlights(Editor& ed, const std::string& text, const TextGeometry& geo) {
    if (!geo.valid || !geo.drawList) return;
    ColorScheme& c = theme::colors();
    ImDrawList* dl = geo.drawList;
    dl->PushClipRect(geo.clip.Min, geo.clip.Max, true);

    for (const ManuscriptToken& t : parseManuscript(ed.project, text)) {
        if (t.kind == ManuscriptToken::Kind::Text) continue;
        size_t begin = t.begin;
        size_t end = std::min(t.end, text.size());
        while (end > begin && (text[end - 1] == '\n' || text[end - 1] == '\r')) --end;
        if (end <= begin) continue;

        const size_t line = lineOfOffset(geo, begin);
        const float y = geo.origin.y + static_cast<float>(line) * geo.lineHeight;
        if (y + geo.lineHeight < geo.clip.Min.y || y > geo.clip.Max.y) continue;  // nicht sichtbar

        const size_t start = geo.lineStarts[line];
        const float x0 =
            geo.origin.x + ImGui::CalcTextSize(text.c_str() + start, text.c_str() + begin).x;
        const float x1 = x0 + ImGui::CalcTextSize(text.c_str() + begin, text.c_str() + end).x;

        ImVec4 col = c.accentColor;
        switch (t.kind) {
            case ManuscriptToken::Kind::Heading: col = c.accentColor; break;
            case ManuscriptToken::Kind::Time: col = c.timelineRuler; break;
            case ManuscriptToken::Kind::Action:
                col = t.resolved ? c.warningColor : c.errorColor;
                break;
            default:
                col = t.resolved ? ed.project.elementColor(t.targetId) : c.errorColor;
                break;
        }

        const float thickness = t.kind == ManuscriptToken::Kind::Value ? 2.5f : 1.5f;
        dl->AddRectFilled(ImVec2(x0 - 2.0f, y), ImVec2(x1 + 2.0f, y + geo.lineHeight),
                          theme::u32(theme::withAlpha(col, 0.16f)), 3.0f);
        dl->AddLine(ImVec2(x0 - 2.0f, y + geo.lineHeight - 1.0f),
                    ImVec2(x1 + 2.0f, y + geo.lineHeight - 1.0f), theme::u32(col), thickness);
    }
    dl->PopClipRect();
}

// Fliesstext mit eingebetteten, anklickbaren Marken.
void drawRendered(Editor& ed, const std::string& text, bool clickable, bool showTimes) {
    ColorScheme& c = theme::colors();
    const float wrapX = ImGui::GetContentRegionAvail().x;
    const float spaceW = ImGui::CalcTextSize(" ").x;
    float x = 0.0f;
    ImGui::BeginGroup();

    auto newLine = [&]() {
        ImGui::NewLine();
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
    };

    for (const ManuscriptToken& t : parseManuscript(ed.project, text)) {
        switch (t.kind) {
            case ManuscriptToken::Kind::Heading: {
                newLine();
                ImGui::Spacing();
                ImGui::PushStyleColor(ImGuiCol_Text, c.accentColor);
                ImGui::PushFont(nullptr, ImGui::GetFontSize() * 1.2f);
                std::string title = trim(t.raw.substr(t.raw.find_first_not_of('#') == std::string::npos
                                                          ? t.raw.size()
                                                          : t.raw.find_first_not_of('#')));
                ImGui::TextUnformatted(title.c_str());
                ImGui::PopFont();
                ImGui::PopStyleColor();
                x = 0.0f;
                break;
            }
            case ManuscriptToken::Kind::Time: {
                if (!showTimes) break;
                newLine();
                ImGui::PushStyleColor(ImGuiCol_Text, c.textSecondary);
                ImGui::TextUnformatted(("--- " + formatStoryTime(t.time) + " ---").c_str());
                ImGui::PopStyleColor();
                x = 0.0f;
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
    }
    ImGui::EndGroup();
}

// Aus dem Absatz um den Cursor eine Aktion machen.
void actionFromParagraph(Editor& ed, ManuscriptState& st) {
    std::string& text = ed.project.manuscript;
    size_t begin = 0, end = 0;
    paragraphAt(text, static_cast<size_t>(std::max(0, st.cursor)), &begin, &end);
    const std::string paragraph = text.substr(begin, end - begin);
    if (trim(paragraph).empty()) return;

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

    // ------------------------------------------------------------- toolbar
    if (ImGui::RadioButton(TR("Schreiben"), !st.readMode)) st.readMode = false;
    ImGui::SameLine();
    if (ImGui::RadioButton(TR("Lesen"), st.readMode)) st.readMode = true;
    ImGui::SameLine();
    ImGui::Checkbox(TR("Gliederung"), &st.showOutline);
    if (st.readMode) {
        ImGui::SameLine();
        ImGui::Checkbox(TR("Marken zeigen"), &st.showMarks);
        ui::tooltip(TR("Nur zur Orientierung beim Lesen - im Export steht sie nie."));
    }
    ImGui::SameLine();
    if (ImGui::Button(TR("Aktion aus Absatz"))) actionFromParagraph(ed, st);
    ui::tooltip(TR("Macht aus dem Absatz am Cursor eine Aktion - Beteiligte und Zeitpunkt kommen "
                   "aus dem Text."));
    ImGui::SameLine();
    const long long timeHereForAction =
        timeAtOffset(ed.project, text, static_cast<size_t>(std::max(0, st.cursor)));
    if (ImGui::Button(TR("Aktion setzen"))) {
        st.actionSearch.clear();
        ImGui::OpenPopup("ms_action");
    }
    ui::tooltip(TR("Setzt an dieser Stelle eine Marke auf eine Aktion - im fertigen Text "
                   "unsichtbar, dient nur der Organisation. Kurzform beim Tippen: !"));
    if (ImGui::BeginPopup("ms_action")) {
        ui::textSecondary((TR("Hier gilt: ") + formatStoryTime(timeHereForAction)).c_str());
        ImGui::SetNextItemWidth(260.0f);
        ImGui::InputTextWithHint("##actsearch", TR("Aktion suchen..."), &st.actionSearch);
        ImGui::Separator();
        int shown = 0;
        for (const Action* a : ed.project.sortedActions()) {
            if (!st.actionSearch.empty() && !iequalsContains(a->title, st.actionSearch)) continue;
            ImGui::PushID(a->id.c_str());
            ui::textSecondary(formatStoryTime(ed.project.resolveActionTime(*a)).c_str());
            ImGui::SameLine();
            if (ImGui::Selectable(a->title.c_str())) {
                insertAtCursor(st, " !act:" + a->id);
                ed.markManuscript();
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopID();
            if (++shown >= 10) break;
        }
        if (shown == 0) ui::textSecondary(TR("Keine passende Aktion."));
        ImGui::Separator();
        if (ImGui::Button(TR("+ Neue Aktion hier"))) {
            ed.pushUndo(TR("Aktion erstellt"));
            const std::string title =
                st.actionSearch.empty() ? std::string(TR("Neue Aktion")) : st.actionSearch;
            Action& a = ed.project.addAction(title, timeHereForAction);
            if (!ed.project.actionTypes.empty()) a.type = ed.project.actionTypes.front();
            size_t begin = 0, end = 0;
            paragraphAt(text, static_cast<size_t>(std::max(0, st.cursor)), &begin, &end);
            a.elementIds = mentionedElements(ed.project, text.substr(begin, end - begin));
            insertAtCursor(st, " !act:" + a.id);
            ed.markActions();
            ed.markManuscript();
            ed.select(SelKind::Action, a.id);
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::SameLine();
    const long long timeHere =
        timeAtOffset(ed.project, text, static_cast<size_t>(std::max(0, st.cursor)));
    if (ImGui::Button(TR("Zeitmarke"))) {
        st.timeDraft = formatStoryTime(timeHere);
        ImGui::OpenPopup("ms_time");
    }
    ui::tooltip(TR("Ab dieser Stelle gilt ein neuer Zeitpunkt - Werte im Text richten sich danach. "
                   "Im fertigen Text ist die Marke nicht zu sehen."));
    if (ImGui::BeginPopup("ms_time")) {
        ui::textSecondary((TR("Hier gilt: ") + formatStoryTime(timeHere)).c_str());
        ImGui::Separator();
        struct Quick {
            const char* label;
            long long offset;
        };
        const Quick quick[] = {
            {"+ 1 h", kMinutesPerHour},           {"+ 6 h", 6 * kMinutesPerHour},
            {"+ 1 Tag", kMinutesPerDay},          {"+ 3 Tage", 3 * kMinutesPerDay},
            {"+ 1 Woche", 7 * kMinutesPerDay},    {"+ 1 Monat", 30 * kMinutesPerDay},
        };
        int column = 0;
        for (const Quick& q : quick) {
            if (column++ % 3 != 0) ImGui::SameLine();
            if (ImGui::Button(TR(q.label), ImVec2(90, 0))) {
                insertAtCursor(st, "\n#" + formatStoryTime(timeHere + q.offset) + "\n");
                ed.markManuscript();
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::Separator();
        ImGui::SetNextItemWidth(180.0f);
        const bool entered =
            ImGui::InputText("##timedraft", &st.timeDraft, ImGuiInputTextFlags_EnterReturnsTrue);
        long long parsed = 0;
        const bool ok = parseStoryTime(st.timeDraft, &parsed);
        ImGui::SameLine();
        if (!ok) ImGui::BeginDisabled();
        if ((ImGui::Button(TR("Einfuegen")) || (entered && ok)) && ok) {
            insertAtCursor(st, "\n#" + formatStoryTime(parsed) + "\n");
            ed.markManuscript();
            ImGui::CloseCurrentPopup();
        }
        if (!ok) ImGui::EndDisabled();
        ImGui::EndPopup();
    }

    ImGui::SameLine();
    if (ImGui::Button(TR("Als Word"))) ed.exportManuscriptToWord();
    ui::tooltip(TR("Schreibt die fertige Geschichte: Werte sind fest eingesetzt, "
                   "Zeit- und Aktionsmarken stehen nicht darin."));

    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, c.textSecondary);
    ImGui::Text("%zu %s", countWords(text), TR("Woerter"));
    ImGui::PopStyleColor();

    ImGui::Separator();

    // ------------------------------------------------------------- outline
    if (st.showOutline) {
        ImGui::BeginChild("outline", ImVec2(190, 0), ImGuiChildFlags_Borders);
        ImGui::TextUnformatted(TR("Gliederung"));
        ImGui::Separator();
        bool any = false;
        for (const ManuscriptToken& t : parseManuscript(ed.project, text)) {
            if (t.kind == ManuscriptToken::Kind::Heading) {
                any = true;
                std::string title = t.raw;
                size_t firstText = title.find_first_not_of("# ");
                title = firstText == std::string::npos ? title : title.substr(firstText);
                if (ImGui::Selectable(title.c_str())) st.cursor = static_cast<int>(t.begin);
            } else if (t.kind == ManuscriptToken::Kind::Time) {
                any = true;
                ImGui::PushStyleColor(ImGuiCol_Text, c.textSecondary);
                ImGui::BulletText("%s", formatStoryTime(t.time).c_str());
                ImGui::PopStyleColor();
            }
        }
        if (!any)
            ui::textSecondary(TR("Ueberschriften mit ## und Zeitmarken mit # erscheinen hier."));
        ImGui::EndChild();
        ImGui::SameLine();
    }

    // -------------------------------------------------------------- pages
    ImGui::BeginChild("page", ImVec2(0, 0), ImGuiChildFlags_Borders);
    if (st.readMode) {
        drawRendered(ed, text, true, st.showMarks);
    } else {
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

    // Hilfe fuer den Anfang
    if (trim(text).empty() && !st.readMode) {
        ui::textSecondary(TR("Einfach losschreiben. @Name verweist auf ein Element, @Name.feld "
                             "setzt dessen Wert ein, #Tag 5 setzt den Zeitpunkt."));
    }
    ImGui::End();
}

}  // namespace se
