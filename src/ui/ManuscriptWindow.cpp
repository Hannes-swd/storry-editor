// Manuskript-Fenster: hier wird die Geschichte geschrieben - in einer
// Oberflaeche, die sich wie Word anfuehlt. Oben das Menueband mit
// Registerkarten (Datei, Start, Einfuegen, Layout, Ueberpruefen, Ansicht),
// links der Navigationsbereich, in der Mitte die Seite, unten die
// Statusleiste mit Seitenzahl, Woertern, Zeitpunkt und Zoom.
//
// Das Schreibfeld selbst ist ui/DocumentView: es zeigt den Text so, wie er
// gemeint ist, und schreibt jede Aenderung als Manuskript-Quelltext zurueck
// (siehe core/Manuscript.h). Die Struktur - Figuren, Werte, Zeitpunkte,
// Aktionen - entsteht nebenbei aus Marken im Text.
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_stdlib.h"

#include "app/SpellCheck.h"
#include "core/Manuscript.h"
#include "core/StoryTime.h"
#include "ui/Dialogs.h"
#include "ui/DocumentView.h"
#include "ui/Editor.h"
#include "ui/Lang.h"
#include "ui/Ribbon.h"
#include "ui/Theme.h"
#include "ui/UiCommon.h"
#include "ui/Windows.h"

namespace se {
namespace {

namespace icon = ribbon::icon;
constexpr size_t kNone = static_cast<size_t>(-1);

enum Tab { TabFile, TabHome, TabInsert, TabLayout, TabReview, TabView, TabCount };

struct ManuscriptState {
    ManuscriptDoc doc;
    DocumentView main{"##page_main"};
    DocumentView second{"##page_second"};
    bool registered = false;
    bool split = false;
    float splitRatio = 0.5f;
    bool readMode = false;
    bool focusMode = false;
    bool showHelp = false;

    // ------------------------------------------------------------- Suchen
    bool showFind = false;
    bool findFocus = false;
    bool showReplace = false;
    std::string findQuery;
    std::string replaceQuery;
    bool findCase = false;
    int findHit = 0;

    // ------------------------------------------------------------ Einfuegen
    std::string actionSearch;
    std::string elementSearch;
    std::string timeDraft;
    std::string sizeDraft;
    std::string textColor;       // zuletzt benutzte Schriftfarbe
    std::string highlightColor;  // zuletzt benutzte Hervorhebung
    ImVec4 customColor = ImVec4(0.5f, 0.2f, 0.2f, 1.0f);

    // ------------------------------------------ Kommentar / Lesezeichen
    bool openNote = false;
    std::string noteDraft;
    size_t noteBegin = kNone, noteEnd = kNone;
    bool openBookmark = false;
    std::string bookmarkDraft;
    bool openWordCount = false;
    bool openContext = false;
    // Rechtsklick auf ein rot markiertes Wort
    size_t spellBegin = 0, spellEnd = 0;
    std::string spellWord;
    std::vector<std::string> spellSuggestions;
    uint64_t namesSignature = 0;  // Elementnamen fuer die Rechtschreibung

    // ---------------------------------------------- Autovervollstaendigung
    std::string completionWord;
    char completionSigil = 0;
    size_t completionStart = kNone;
    int completionPick = 0;
    bool completionOpen = false;
    bool pickFiltered = false;
    bool pickMoved = false;
    std::string pickText;
    bool muted = false;
    std::string mutedWord;
    uint64_t completionGeneration = 0;  // Textstand im letzten Frame
    size_t completionCursor = 0;        // Cursor im letzten Frame

    // ---------------------------------------------------------- Zwischenstand
    uint64_t wordsGeneration = 0;
    size_t words = 0;
};

ManuscriptState& state() {
    static ManuscriptState s;
    return s;
}

DocumentView& activeView(ManuscriptState& st) {
    return st.split && st.second.activeView ? st.second : st.main;
}

std::string toHex(const ImVec4& c) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "#%02X%02X%02X", static_cast<int>(c.x * 255.0f + 0.5f),
                  static_cast<int>(c.y * 255.0f + 0.5f), static_cast<int>(c.z * 255.0f + 0.5f));
    return buf;
}

ImVec4 fromHex(const std::string& hex) {
    if (hex.size() != 7 || hex[0] != '#') return ImVec4(0, 0, 0, 1);
    const unsigned long v = std::strtoul(hex.c_str() + 1, nullptr, 16);
    return ImVec4(static_cast<float>((v >> 16) & 0xFF) / 255.0f, static_cast<float>((v >> 8) & 0xFF) / 255.0f,
                  static_cast<float>(v & 0xFF) / 255.0f, 1.0f);
}

std::string formatPt(float v) {
    char buf[16];
    if (std::fabs(v - std::round(v)) < 0.01f)
        std::snprintf(buf, sizeof(buf), "%d", static_cast<int>(std::round(v)));
    else
        std::snprintf(buf, sizeof(buf), "%.1f", v);
    return buf;
}

// ------------------------------------------------------------ Sprache
// Einstellung "de"/"en" oder eine genaue Variante wie "en-GB". Windows bietet
// nicht jede Variante an - dann wird die naechstbeste installierte genommen.
const std::vector<std::string>& installedLanguages() {
    static std::vector<std::string> list = spell::installedLanguages();
    return list;
}

bool languageInstalled(const std::string& tag) {
    const std::vector<std::string>& installed = installedLanguages();
    return std::find(installed.begin(), installed.end(), tag) != installed.end();
}

std::string resolveLanguage(const std::string& setting) {
    const std::vector<std::string>& installed = installedLanguages();
    auto has = [&](const std::string& tag) {
        return std::find(installed.begin(), installed.end(), tag) != installed.end();
    };
    if (setting.size() > 2 && has(setting)) return setting;
    const std::string base = setting.substr(0, 2);
    const char* preferred[] = {"de-DE", "de-AT", "de-CH", "en-US", "en-GB"};
    for (const char* p : preferred) {
        if (std::string(p).compare(0, 2, base) == 0 && has(p)) return p;
    }
    for (const std::string& tag : installed) {
        if (tag.compare(0, 2, base) == 0) return tag;
    }
    return base == "en" ? "en-US" : "de-DE";
}

std::string languageName(const std::string& tag) {
    struct Name {
        const char* tag;
        const char* name;
    };
    static const Name names[] = {
        {"de-DE", "Deutsch (Deutschland)"}, {"de-AT", "Deutsch (Oesterreich)"}, {"de-CH", "Deutsch (Schweiz)"},
        {"de-LI", "Deutsch (Liechtenstein)"}, {"de-LU", "Deutsch (Luxemburg)"}, {"en-US", "Englisch (USA)"},
        {"en-GB", "Englisch (Grossbritannien)"}, {"en-AU", "Englisch (Australien)"}, {"en-CA", "Englisch (Kanada)"},
        {"en-IE", "Englisch (Irland)"}, {"en-IN", "Englisch (Indien)"}, {"en-NZ", "Englisch (Neuseeland)"},
        {"en-ZA", "Englisch (Suedafrika)"},
    };
    for (const Name& n : names) {
        if (tag == n.tag) return TR(n.name);
    }
    return tag;
}

// Auswahl der Textsprache - im Menueband und in der Statusleiste.
bool languageMenu() {
    AppSettings& s = theme::settings();
    bool changed = false;
    const std::string current = resolveLanguage(s.docLanguage);
    if (ImGui::MenuItem(TR("Deutsch"), nullptr, current.compare(0, 2, "de") == 0)) {
        s.docLanguage = "de";
        changed = true;
    }
    if (ImGui::MenuItem(TR("Englisch"), nullptr, current.compare(0, 2, "en") == 0)) {
        s.docLanguage = "en";
        changed = true;
    }
    if (ImGui::BeginMenu(TR("Varianten"))) {
        for (const std::string& tag : installedLanguages()) {
            if (tag.compare(0, 2, "de") != 0 && tag.compare(0, 2, "en") != 0) continue;
            if (ImGui::MenuItem(languageName(tag).c_str(), tag.c_str(), tag == current)) {
                s.docLanguage = tag;
                changed = true;
            }
        }
        ImGui::EndMenu();
    }
    if (!languageInstalled(current)) {
        ImGui::Separator();
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 320.0f);
        ui::textSecondary(TR("Diese Sprache ist in Windows nicht installiert. Hinzufuegen unter "
                             "Einstellungen > Zeit und Sprache > Sprache und Region."));
        ImGui::PopTextWrapPos();
    }
    if (changed) theme::save();
    return changed;
}

DocOptions makeOptions(const ManuscriptState& st) {
    const AppSettings& s = theme::settings();
    DocOptions o;
    o.readMode = st.readMode;
    o.showMarks = s.docShowMarks;
    o.pageView = s.docPageView;
    o.zoom = s.docZoom;
    theme::pageSizeCm(s.docPageFormat, &o.pageWidthCm, &o.pageHeightCm);
    o.marginCm = s.docMarginCm;
    o.marginLeftCm = s.docMarginLeftCm;
    o.marginRightCm = s.docMarginRightCm;
    o.font = s.docFont;
    o.fontPt = s.docFontSize;
    o.lineSpacing = s.docLineSpacing;
    o.spellCheck = s.docSpellCheck;
    o.autoCorrect = s.docAutoCorrect;
    o.language = resolveLanguage(s.docLanguage);
    return o;
}

// Zeitpunkt, der an einer Stelle gilt - aus dem schon geparsten Text.
long long timeAt(ManuscriptDoc& doc, size_t pos) {
    long long time = 0;
    for (const ManuscriptToken& t : doc.tokens()) {
        if (t.begin >= pos) break;
        if (t.kind == ManuscriptToken::Kind::Time) time = t.time;
    }
    return time;
}

// Nach jeder Schaltflaeche im Band gehoert die Tastatur wieder der Seite.
void done(DocumentView& view) {
    view.requestFocus();
    view.requestScrollToCaret();
}

void toggleStyle(ManuscriptDoc& doc, DocumentView& view, bool TextStyle::*flag) {
    const bool on = docops::selectionHas(doc, view, [&](const TextStyle& s) { return s.*flag; });
    docops::applyStyle(doc, view, [&](TextStyle& s) {
        s.*flag = !on;
        if (!on && flag == &TextStyle::superscript) s.subscript = false;
        if (!on && flag == &TextStyle::subscript) s.superscript = false;
    });
    done(view);
}

bool styleActive(ManuscriptDoc& doc, DocumentView& view, bool TextStyle::*flag) {
    return docops::selectionHas(doc, view, [&](const TextStyle& s) { return s.*flag; });
}

void setFontSize(ManuscriptDoc& doc, DocumentView& view, float pt) {
    const float base = theme::settings().docFontSize;
    pt = std::clamp(pt, 4.0f, 144.0f);
    docops::applyStyle(doc, view, [&](TextStyle& s) { s.size = std::fabs(pt - base) < 0.01f ? 0.0f : pt; });
}

void growFont(ManuscriptDoc& doc, DocumentView& view, int dir) {
    static const float steps[] = {8, 9, 10, 11, 12, 14, 16, 18, 20, 22, 24, 26, 28, 36, 48, 72};
    const float base = theme::settings().docFontSize;
    const float cur = docops::caretStyle(doc, view).size;
    const float from = cur > 0.0f ? cur : base;
    float target = from;
    if (dir > 0) {
        target = from + 1.0f;
        for (float s : steps) {
            if (s > from + 0.01f) {
                target = s;
                break;
            }
        }
    } else {
        target = std::max(1.0f, from - 1.0f);
        for (int i = static_cast<int>(sizeof(steps) / sizeof(steps[0])) - 1; i >= 0; --i) {
            if (steps[i] < from - 0.01f) {
                target = steps[i];
                break;
            }
        }
    }
    setFontSize(doc, view, target);
}

// ---------------------------------------------------------------- Auftraege
void insertTimeMark(ManuscriptDoc& doc, DocumentView& view, long long when) {
    docops::insertOwnLine(doc, view, "#" + formatStoryTime(when));
    done(view);
}

// Aus dem Absatz um den Cursor eine Aktion machen.
void actionFromParagraph(Editor& ed, ManuscriptState& st, DocumentView& view) {
    const std::string& text = ed.project.manuscript;
    size_t begin = 0, end = 0;
    paragraphAt(text, view.cursor, &begin, &end);
    const std::string paragraph = text.substr(begin, end - begin);
    if (trim(paragraph).empty()) {
        ed.setStatus(TR("Der Absatz am Cursor ist leer."), true);
        return;
    }

    const long long time = timeAt(st.doc, begin);
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
    const std::string id = a.id;

    // Marke ans Ende des Absatzes (vor eine Ausrichtungsmarke der letzten Zeile)
    const std::vector<ManuscriptLine>& lines = st.doc.lines();
    const ManuscriptLine& last = lines[lineIndexAt(lines, end > 0 ? end - 1 : 0)];
    const size_t at = std::min(end, last.contentEnd);
    st.doc.replace(at, 0, " !act:" + id, &view, view.cursor, view.anchor);
    ed.markActions();
    ed.select(SelKind::Action, id);
    ed.setStatus(TR("Aktion angelegt: ") + title);
}

// ------------------------------------------------------------------ Menues
// Elementliste mit Suchfeld - einmal fuer "@Name" und einmal fuer "@Name.feld".
// Im zweiten Fall klappt jedes Element seine Felder als Untermenue auf, und
// daneben steht gleich der Wert, der zu diesem Zeitpunkt gilt.
void elementPickerMenu(Editor& ed, ManuscriptState& st, DocumentView& view, bool withField,
                       long long timeHere) {
    ImGui::SetNextItemWidth(220.0f);
    if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
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
                        key + (value.empty() ? std::string("   ") + TR("(leer)")
                                             : "   " + ui::ellipsis(value, 24));
                    if (ImGui::MenuItem(label.c_str())) {
                        docops::insertReference(st.doc, view, "@" + el.name + "." + key);
                        done(view);
                        ImGui::CloseCurrentPopup();
                    }
                    ++fields;
                }
                if (fields == 0) ui::textSecondary(TR("Dieses Element hat keine Felder."));
                ImGui::EndMenu();
            }
        } else if (ImGui::MenuItem(el.name.c_str())) {
            docops::insertReference(st.doc, view, "@" + el.name);
            done(view);
        }
        ui::tooltip(ed.project.elementPath(el.id).c_str());
        ImGui::PopID();
        if (++shown >= 14) break;
    }
    if (shown == 0) ui::textSecondary(TR("Nichts gefunden."));
}

void actionPickerMenu(Editor& ed, ManuscriptState& st, DocumentView& view, long long timeHere) {
    ImGui::SetNextItemWidth(260.0f);
    if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
    ImGui::InputTextWithHint("##actsearch", TR("Aktion suchen..."), &st.actionSearch);
    ImGui::Separator();
    int shown = 0;
    for (const Action* a : ed.project.sortedActions()) {
        if (!st.actionSearch.empty() && !iequalsContains(a->title, st.actionSearch)) continue;
        ImGui::PushID(a->id.c_str());
        ui::textSecondary(formatStoryTime(ed.project.resolveActionTime(*a)).c_str());
        ImGui::SameLine();
        if (ImGui::MenuItem(a->title.c_str())) {
            docops::insertSource(st.doc, view, " !act:" + a->id);
            done(view);
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
        paragraphAt(ed.project.manuscript, view.cursor, &begin, &end);
        a.elementIds =
            mentionedElements(ed.project, ed.project.manuscript.substr(begin, end - begin));
        const std::string id = a.id;
        docops::insertSource(st.doc, view, " !act:" + id);
        ed.markActions();
        ed.select(SelKind::Action, id);
        done(view);
        ImGui::CloseCurrentPopup();
    }
    ui::tooltip(TR("Legt eine Aktion zum Zeitpunkt dieser Stelle an und verknuepft sie."));
}

void timeMenu(ManuscriptState& st, DocumentView& view, long long timeHere) {
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 360.0f);
    ImGui::TextWrapped("%s", TR("Die Zeit laeuft nicht von allein weiter: sie bleibt stehen, "
                                "bis du sie weiterstellst. Ab der Marke gilt der neue "
                                "Zeitpunkt fuer alles, was danach im Text kommt."));
    ImGui::PopTextWrapPos();
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
            insertTimeMark(st.doc, view, timeHere + q.offset);
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
        insertTimeMark(st.doc, view, parsed);
        ImGui::CloseCurrentPopup();
    }
    if (!ok) ImGui::EndDisabled();
    ui::textSecondary(TR("z.B. \"Tag 5, 14:00\" oder \"Jahr 2, Monat 3, Tag 15\""));
}

// Farbauswahl wie in Word: Palette, "Automatisch"/"Keine Farbe", eigene Farbe.
bool colorPalette(ManuscriptState& st, const std::vector<ImVec4>& palette, const char* noneLabel,
                  std::string* picked) {
    bool chosen = false;
    if (ImGui::Selectable(noneLabel)) {
        picked->clear();
        chosen = true;
    }
    ImGui::Separator();
    const float sz = ImGui::GetFrameHeight();
    for (size_t i = 0; i < palette.size(); ++i) {
        if (i % 5 != 0) ImGui::SameLine();
        ImGui::PushID(static_cast<int>(i));
        if (ImGui::ColorButton("##sw", palette[i], ImGuiColorEditFlags_NoTooltip, ImVec2(sz, sz))) {
            *picked = toHex(palette[i]);
            chosen = true;
        }
        ImGui::PopID();
    }
    ImGui::Separator();
    if (ImGui::BeginMenu(TR("Weitere Farben..."))) {
        ImGui::ColorPicker3("##custom", reinterpret_cast<float*>(&st.customColor),
                            ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoSmallPreview);
        if (ImGui::Button(TR("Uebernehmen"), ImVec2(-1, 0))) {
            *picked = toHex(st.customColor);
            chosen = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndMenu();
    }
    if (chosen) ImGui::CloseCurrentPopup();
    return chosen;
}

// Formatvorlage im Katalog: ein kleines Blatt mit einer Probe der Schrift.
bool styleSample(const char* id, const char* name, float samplePt, bool bold, bool active,
                 const std::string& family, const char* tip) {
    const ColorScheme& c = theme::colors();
    const float h = ribbon::rowHeight() * 2.0f + ImGui::GetStyle().ItemSpacing.y;
    const float w = h * 1.55f;
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const bool pressed = ImGui::InvisibleButton(id, ImVec2(w, h));
    const bool hovered = ImGui::IsItemHovered();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 q(p.x + w, p.y + h);
    dl->AddRectFilled(p, q, theme::u32(c.pageColor), 3.0f);
    dl->AddRect(p, q, theme::u32(active ? c.accentColor : theme::withAlpha(c.textSecondary, hovered ? 0.8f : 0.35f)),
                3.0f, 0, active ? 2.0f : 1.0f);
    ImFont* f = theme::familyFont(family, bold, false);
    const float px = std::min(h * 0.42f, samplePt * 1.1f);
    const char* sample = "AaBbCc";
    const ImVec2 ts = f->CalcTextSizeA(px, FLT_MAX, 0.0f, sample);
    dl->PushClipRect(p, q, true);
    dl->AddText(f, px, ImVec2(p.x + 5.0f, p.y + (h * 0.62f - ts.y) * 0.5f + 2.0f), theme::u32(c.pageTextColor), sample);
    const ImVec2 ns = ImGui::CalcTextSize(name);
    dl->AddText(ImVec2(p.x + (w - ns.x) * 0.5f, q.y - ns.y - 3.0f), theme::u32(theme::withAlpha(c.pageTextColor, 0.7f)), name);
    dl->PopClipRect();
    if (tip && hovered) ImGui::SetTooltip("%s", tip);
    return pressed;
}

// ------------------------------------------------------------------ Hilfe
void drawHelpWindow(ManuscriptState& st) {
    if (!st.showHelp) return;
    ImGui::SetNextWindowSize(ImVec2(520, 0), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(TWIN("Wie das Schreiben hier funktioniert", "manuscript_help"), &st.showHelp,
                     ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::PushTextWrapPos(500.0f);
        ImGui::TextWrapped("%s",
                           TR("Schreib einfach los wie in Word. Das Menueband oben formatiert, "
                              "was du markiert hast - ohne Auswahl gilt das Format fuer das, "
                              "was du als Naechstes tippst."));
        ImGui::Spacing();
        ImGui::Separator();

        struct Row {
            const char* what;
            const char* how;
        };
        const Row rows[] = {
            {"Formatieren", "Start: Schriftart, Groesse, Fett (Strg+B), Kursiv (Strg+I), "
                            "Unterstrichen (Strg+U), Farbe, Hervorhebung."},
            {"Ueberschriften", "Start > Formatvorlagen: Titel, Kapitel, Szene. Sie erscheinen links "
                               "im Navigationsbereich."},
            {"Ausrichten und Listen", "Start > Absatz: links, zentriert (Strg+E), rechts (Strg+R), "
                                      "Blocksatz (Strg+J), Aufzaehlung, Nummerierung."},
            {"Eine Figur, ein Ort, ein Ding nennen",
             "Einfuegen > Element - oder einfach @ tippen und weiterschreiben."},
            {"Einen Wert einsetzen (Alter, Titel, Zustand)",
             "Einfuegen > Wert. Im Text steht dann immer der Wert, der zu diesem Zeitpunkt "
             "der Geschichte gilt."},
            {"Zeit vergehen lassen",
             "Einfuegen > Zeitpunkt. Die Zeit bleibt stehen, bis du sie weiterstellst."},
            {"Eine Stelle als Ereignis merken",
             "Einfuegen > Aus Absatz - Beteiligte und Zeitpunkt kommen aus dem Text."},
            {"Lesezeichen und Kommentare",
             "Ueberpruefen: Kommentar an die Stelle haengen, Lesezeichen setzen und anspringen."},
            {"Seite und Ansicht", "Layout: Raender, Format, Zeilenabstand. Ansicht: Lesemodus, "
                                  "Lineal, Zoom (Strg+Mausrad), geteilte Ansicht, Fokus."},
            {"Rechtschreibung und AutoKorrektur",
             "Ueberpruefen > Rechtschreibung: rote Wellen unter unbekannten Woertern, Rechtsklick "
             "zeigt Vorschlaege, F7 springt zum naechsten. AutoKorrektur und Sprache daneben oder "
             "unten in der Statusleiste."},
            {"Lineal: Einzuege und Tabstopps",
             "Dreiecke im Lineal ziehen: Einzug der ersten Zeile, haengender und rechter Einzug. "
             "Klick ins Lineal setzt einen Tabstopp, herausziehen entfernt ihn. Die Grenze grau/weiss "
             "verschiebt den Seitenrand."},
            {"Etwas wiederfinden", "Suchen (Strg+F), Ersetzen (Strg+H) - findet auch Namen und Marken."},
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
        ImGui::TextWrapped("%s", TR("Marken, Lesezeichen und Kommentare sind nur beim Schreiben zu "
                                    "sehen. Im Lesemodus und in der Word-Datei steht der fertige Text."));
        ImGui::PopTextWrapPos();
    }
    ImGui::End();
}

// ------------------------------------------------------------------ Suchen
void drawFindBar(ManuscriptState& st, DocumentView& view, const std::vector<size_t>& hits) {
    ManuscriptDoc& doc = st.doc;
    const ColorScheme& c = theme::colors();
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
        const size_t at = hits[static_cast<size_t>(st.findHit)];
        view.select(at, at + st.findQuery.size());
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
    const bool canReplace = !hits.empty() && !st.readMode;
    if (!canReplace) ImGui::BeginDisabled();
    if (ImGui::Button(TR("Ersetzen"))) {
        const size_t at = hits[static_cast<size_t>(std::min<int>(st.findHit, static_cast<int>(hits.size()) - 1))];
        docops::replaceRaw(doc, view, at, at + st.findQuery.size(), st.replaceQuery);
    }
    ImGui::SameLine();
    if (ImGui::Button(TR("Alle ersetzen"))) {
        const std::string& text = doc.text();
        std::string out;
        size_t last = 0;
        for (size_t at : hits) {
            out += text.substr(last, at - last);
            out += st.replaceQuery;
            last = at + st.findQuery.size();
        }
        out += text.substr(last);
        const size_t count = hits.size();
        doc.replace(0, text.size(), out, &view, 0, 0);
        doc.editor().setStatus(std::to_string(count) + TR(" Stellen ersetzt."));
        st.findHit = 0;
    }
    if (!canReplace) ImGui::EndDisabled();

    ImGui::SameLine();
    if (ImGui::Button(TR("Schliessen"))) {
        st.showFind = false;
        done(view);
    }

    // F3 und Enter im Suchfeld springen weiter.
    const bool enter = queryActive && (ImGui::IsKeyPressed(ImGuiKey_Enter, false) ||
                                       ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false));
    if (ImGui::IsKeyPressed(ImGuiKey_F3, false) || enter)
        goTo(ImGui::GetIO().KeyShift ? st.findHit - 1 : st.findHit + 1);
    if (queryActive && ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
        st.showFind = false;
        done(view);
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ---------------------------------------------------------- Menueband
void ribbonHome(Editor& ed, ManuscriptState& st, DocumentView& view, const DocOptions& opt) {
    ManuscriptDoc& doc = st.doc;
    const ColorScheme& c = theme::colors();
    const bool writing = !st.readMode;
    const TextStyle caret = docops::caretStyle(doc, view);
    const ManuscriptLine* line = docops::caretLine(doc, view);
    (void)ed;

    if (!writing) ImGui::BeginDisabled();

    // ------------------------------------------------------ Zwischenablage
    ribbon::beginGroup();
    if (ribbon::big("##paste", icon::paste, TR("Einfuegen"), TR("Aus der Zwischenablage einfuegen (Strg+V)"))) {
        docops::paste(doc, view);
        done(view);
    }
    ImGui::SameLine();
    ImGui::BeginGroup();
    if (ribbon::labeled("##cut", icon::cut, TR("Ausschneiden"), TR("Strg+X"), false, view.hasSelection())) {
        docops::cut(doc, view);
        done(view);
    }
    if (ribbon::labeled("##copy", icon::copy, TR("Kopieren"), TR("Strg+C"), false, view.hasSelection())) {
        docops::copy(doc, view);
        done(view);
    }
    ImGui::EndGroup();
    ribbon::endGroup(TR("Zwischenablage"));

    // ------------------------------------------------------------ Schriftart
    ribbon::beginGroup();
    {
        const std::string family = caret.font.empty() ? opt.font : caret.font;
        ImGui::SetNextItemWidth(ImGui::GetFontSize() * 9.0f);
        if (ImGui::BeginCombo("##font", family.c_str(), ImGuiComboFlags_HeightLarge)) {
            for (const std::string& name : theme::fontFamilies()) {
                ImGui::PushFont(theme::familyFont(name, false, false), 0.0f);
                if (ImGui::Selectable(name.c_str(), name == family)) {
                    const std::string value = name == opt.font ? std::string() : name;
                    docops::applyStyle(doc, view, [&](TextStyle& s) { s.font = value; });
                    done(view);
                }
                ImGui::PopFont();
            }
            ImGui::EndCombo();
        }
        ui::tooltip(TR("Schriftart"));
        ImGui::SameLine(0.0f, 3.0f);

        const float size = caret.size > 0.0f ? caret.size : opt.fontPt;
        ImGui::SetNextItemWidth(ImGui::GetFontSize() * 2.6f);
        const bool editing = ImGui::GetActiveID() == ImGui::GetID("##size");
        if (!editing) st.sizeDraft = formatPt(size);
        if (ImGui::InputText("##size", &st.sizeDraft,
                             ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CharsDecimal)) {
            const float v = static_cast<float>(std::atof(st.sizeDraft.c_str()));
            if (v > 0.0f) setFontSize(doc, view, v);
            done(view);
        }
        ui::tooltip(TR("Schriftgrad (Punkt)"));
        ImGui::SameLine(0.0f, 0.0f);
        if (ribbon::dropArrow("##sizes", TR("Schriftgrad waehlen"))) ImGui::OpenPopup("ms_sizes");
        if (ImGui::BeginPopup("ms_sizes")) {
            static const float sizes[] = {8, 9, 10, 10.5f, 11, 12, 14, 16, 18, 20, 22, 24, 26, 28, 36, 48, 72};
            for (float s : sizes) {
                if (ImGui::Selectable(formatPt(s).c_str(), std::fabs(s - size) < 0.01f)) {
                    setFontSize(doc, view, s);
                    done(view);
                }
            }
            ImGui::EndPopup();
        }
        ImGui::SameLine();
        if (ribbon::tool("##grow", icon::grow, TR("Schrift vergroessern (Strg+Umschalt+>)"))) {
            growFont(doc, view, 1);
            done(view);
        }
        ImGui::SameLine(0.0f, 2.0f);
        if (ribbon::tool("##shrink", icon::shrink, TR("Schrift verkleinern (Strg+<)"))) {
            growFont(doc, view, -1);
            done(view);
        }
        ImGui::SameLine();
        if (ribbon::tool("##clear", icon::clearFormat, TR("Formatierung loeschen (Strg+Leertaste)"))) {
            docops::applyStyle(doc, view, [](TextStyle& s) { s = TextStyle(); });
            done(view);
        }

        // zweite Reihe
        if (ribbon::tool("##bold", icon::bold, TR("Fett (Strg+B)"), styleActive(doc, view, &TextStyle::bold)))
            toggleStyle(doc, view, &TextStyle::bold);
        ImGui::SameLine(0.0f, 2.0f);
        if (ribbon::tool("##italic", icon::italic, TR("Kursiv (Strg+I)"), styleActive(doc, view, &TextStyle::italic)))
            toggleStyle(doc, view, &TextStyle::italic);
        ImGui::SameLine(0.0f, 2.0f);
        if (ribbon::tool("##underline", icon::underline, TR("Unterstrichen (Strg+U)"),
                         styleActive(doc, view, &TextStyle::underline)))
            toggleStyle(doc, view, &TextStyle::underline);
        ImGui::SameLine(0.0f, 2.0f);
        if (ribbon::tool("##strike", icon::strike, TR("Durchgestrichen"), styleActive(doc, view, &TextStyle::strike)))
            toggleStyle(doc, view, &TextStyle::strike);
        ImGui::SameLine(0.0f, 2.0f);
        if (ribbon::tool("##sub", icon::subscript, TR("Tiefgestellt (Strg+#)"),
                         styleActive(doc, view, &TextStyle::subscript)))
            toggleStyle(doc, view, &TextStyle::subscript);
        ImGui::SameLine(0.0f, 2.0f);
        if (ribbon::tool("##sup", icon::superscript, TR("Hochgestellt (Strg++)"),
                         styleActive(doc, view, &TextStyle::superscript)))
            toggleStyle(doc, view, &TextStyle::superscript);

        if (st.highlightColor.empty() && !c.highlightPalette.empty()) st.highlightColor = toHex(c.highlightPalette[0]);
        if (st.textColor.empty() && !c.textPalette.empty()) st.textColor = toHex(c.textPalette[0]);
        ImGui::SameLine();
        if (ribbon::colorTool("##hl", icon::highlighter, theme::u32(fromHex(st.highlightColor)),
                              TR("Texthervorhebungsfarbe"))) {
            const std::string col = st.highlightColor;
            docops::applyStyle(doc, view, [&](TextStyle& s) { s.background = col; });
            done(view);
        }
        ImGui::SameLine(0.0f, 0.0f);
        if (ribbon::dropArrow("##hlmenu", TR("Farbe fuer die Hervorhebung waehlen"))) ImGui::OpenPopup("ms_hl");
        if (ImGui::BeginPopup("ms_hl")) {
            std::string picked;
            if (colorPalette(st, c.highlightPalette, TR("Keine Farbe"), &picked)) {
                if (!picked.empty()) st.highlightColor = picked;
                docops::applyStyle(doc, view, [&](TextStyle& s) { s.background = picked; });
                done(view);
            }
            ImGui::EndPopup();
        }
        ImGui::SameLine(0.0f, 4.0f);
        if (ribbon::colorTool("##fc", icon::fontColor, theme::u32(fromHex(st.textColor)), TR("Schriftfarbe"))) {
            const std::string col = st.textColor;
            docops::applyStyle(doc, view, [&](TextStyle& s) { s.color = col; });
            done(view);
        }
        ImGui::SameLine(0.0f, 0.0f);
        if (ribbon::dropArrow("##fcmenu", TR("Schriftfarbe waehlen"))) ImGui::OpenPopup("ms_fc");
        if (ImGui::BeginPopup("ms_fc")) {
            std::string picked;
            if (colorPalette(st, c.textPalette, TR("Automatisch"), &picked)) {
                if (!picked.empty()) st.textColor = picked;
                docops::applyStyle(doc, view, [&](TextStyle& s) { s.color = picked; });
                done(view);
            }
            ImGui::EndPopup();
        }
    }
    ribbon::endGroup(TR("Schriftart"));

    // ---------------------------------------------------------------- Absatz
    ribbon::beginGroup();
    {
        const LineKind kind = line ? line->kind : LineKind::Body;
        const LineAlign align = line ? line->align : LineAlign::Left;
        if (ribbon::tool("##bullets", icon::bullets, TR("Aufzaehlung"), kind == LineKind::Bullet)) {
            docops::setLineKind(doc, view, kind == LineKind::Bullet ? LineKind::Body : LineKind::Bullet, 0);
            done(view);
        }
        ImGui::SameLine(0.0f, 2.0f);
        if (ribbon::tool("##numbers", icon::numbering, TR("Nummerierung"), kind == LineKind::Numbered)) {
            docops::setLineKind(doc, view, kind == LineKind::Numbered ? LineKind::Body : LineKind::Numbered, 0);
            done(view);
        }
        ImGui::SameLine();
        struct AlignButton {
            const char* id;
            ribbon::IconFn icon;
            const char* tip;
            LineAlign align;
        };
        const AlignButton aligns[] = {
            {"##al", icon::alignLeft, "Linksbuendig (Strg+L)", LineAlign::Left},
            {"##ac", icon::alignCenter, "Zentriert (Strg+E)", LineAlign::Center},
            {"##ar", icon::alignRight, "Rechtsbuendig (Strg+R)", LineAlign::Right},
            {"##aj", icon::alignJustify, "Blocksatz (Strg+J)", LineAlign::Justify},
        };
        for (size_t i = 0; i < 4; ++i) {
            if (i) ImGui::SameLine(0.0f, 2.0f);
            if (ribbon::tool(aligns[i].id, aligns[i].icon, TR(aligns[i].tip), align == aligns[i].align)) {
                docops::setAlign(doc, view, aligns[i].align);
                done(view);
            }
        }

        // zweite Reihe
        if (ribbon::tool("##spacing", icon::lineSpacing, TR("Zeilenabstand"))) ImGui::OpenPopup("ms_spacing");
        if (ImGui::BeginPopup("ms_spacing")) {
            AppSettings& s = theme::settings();
            for (float v : {1.0f, 1.15f, 1.5f, 2.0f, 2.5f, 3.0f}) {
                char label[16];
                std::snprintf(label, sizeof(label), "%.2g", v);
                if (ImGui::Selectable(label, std::fabs(s.docLineSpacing - v) < 0.01f)) {
                    s.docLineSpacing = v;
                    theme::save();
                    done(view);
                }
            }
            ImGui::EndPopup();
        }
        ImGui::SameLine(0.0f, 2.0f);
        AppSettings& s = theme::settings();
        if (ribbon::tool("##marks", icon::marks, TR("Marken anzeigen: Zeitpunkte und Aktionen auch im Lesemodus"),
                         s.docShowMarks)) {
            s.docShowMarks = !s.docShowMarks;
            theme::save();
        }
        ImGui::SameLine(0.0f, 2.0f);
        if (ribbon::tool("##break", icon::sceneBreak, TR("Szenenwechsel einfuegen"))) {
            docops::insertOwnLine(doc, view, "---");
            done(view);
        }
    }
    ribbon::endGroup(TR("Absatz"));

    // --------------------------------------------------------- Formatvorlagen
    ribbon::beginGroup();
    {
        const LineKind kind = line ? line->kind : LineKind::Body;
        const int level = line ? line->level : 0;
        struct Sample {
            const char* id;
            const char* name;
            float pt;
            bool bold;
            LineKind kind;
            int level;
            const char* tip;
        };
        const Sample samples[] = {
            {"##st_normal", "Standard", opt.fontPt, false, LineKind::Body, 0, "Fliesstext"},
            {"##st_title", "Titel", 26.0f, true, LineKind::Heading, 1, "Titel des Werks"},
            {"##st_chapter", "Kapitel", 20.0f, true, LineKind::Heading, 2,
             "Kapitelueberschrift - erscheint im Navigationsbereich und in Word als Ueberschrift 1"},
            {"##st_scene", "Szene", 15.0f, true, LineKind::Heading, 3,
             "Szenenueberschrift - eingerueckt im Navigationsbereich, in Word Ueberschrift 2"},
        };
        for (size_t i = 0; i < 4; ++i) {
            if (i) ImGui::SameLine(0.0f, 3.0f);
            const Sample& sm = samples[i];
            const bool active = sm.kind == LineKind::Heading
                                    ? kind == LineKind::Heading && (level == sm.level || (sm.level == 3 && level > 3))
                                    : kind == LineKind::Body;
            if (styleSample(sm.id, TR(sm.name), sm.pt, sm.bold, active, opt.font, TR(sm.tip))) {
                docops::setLineKind(doc, view, sm.kind, sm.level);
                done(view);
            }
        }
    }
    ribbon::endGroup(TR("Formatvorlagen"));

    if (!writing) ImGui::EndDisabled();

    // ------------------------------------------------------------ Bearbeiten
    ribbon::beginGroup();
    if (ribbon::labeled("##find", icon::find, TR("Suchen"), TR("Stellen im Text finden (Strg+F)"), st.showFind && !st.showReplace)) {
        st.showFind = !(st.showFind && !st.showReplace);
        st.showReplace = false;
        st.findFocus = st.showFind;
    }
    if (ribbon::labeled("##replace", icon::replace, TR("Ersetzen"), TR("Suchen und ersetzen (Strg+H)"), st.showFind && st.showReplace, writing)) {
        st.showFind = !(st.showFind && st.showReplace);
        st.showReplace = st.showFind;
        st.findFocus = st.showFind;
    }
    ribbon::endGroup(TR("Bearbeiten"));
}

void ribbonInsert(Editor& ed, ManuscriptState& st, DocumentView& view, long long timeHere) {
    ManuscriptDoc& doc = st.doc;
    if (st.readMode) ImGui::BeginDisabled();

    ribbon::beginGroup();
    if (ribbon::big("##ins_el", icon::element, TR("Element"),
                    TR("Figur, Ort oder Ding einsetzen - schneller: @ tippen"), false, true))
        ImGui::OpenPopup("ms_ins_el");
    if (ImGui::BeginPopup("ms_ins_el")) {
        elementPickerMenu(ed, st, view, false, timeHere);
        ImGui::EndPopup();
    }
    ImGui::SameLine();
    if (ribbon::big("##ins_val", icon::value, TR("Wert"),
                    TR("Wert eines Feldes einsetzen - im Text steht, was zu diesem Zeitpunkt gilt"),
                    false, true))
        ImGui::OpenPopup("ms_ins_val");
    if (ImGui::BeginPopup("ms_ins_val")) {
        elementPickerMenu(ed, st, view, true, timeHere);
        ImGui::EndPopup();
    }
    ribbon::endGroup(TR("Verweise"));

    ribbon::beginGroup();
    if (ribbon::big("##ins_act", icon::action, TR("Aktion"), TR("Aktion mit dieser Stelle verknuepfen"), false, true))
        ImGui::OpenPopup("ms_ins_act");
    if (ImGui::BeginPopup("ms_ins_act")) {
        actionPickerMenu(ed, st, view, timeHere);
        ImGui::EndPopup();
    }
    ImGui::SameLine();
    if (ribbon::big("##ins_para", icon::paragraphAction, TR("Aus Absatz"),
                    TR("Macht aus dem Absatz am Cursor eine Aktion - Beteiligte und Zeitpunkt kommen aus dem Text.")))
        actionFromParagraph(ed, st, view);
    ribbon::endGroup(TR("Handlung"));

    ribbon::beginGroup();
    if (ribbon::big("##ins_time", icon::clock, TR("Zeitpunkt"), TR("Die Zeit der Geschichte ab hier weiterstellen"),
                    false, true)) {
        st.timeDraft = formatStoryTime(timeHere);
        ImGui::OpenPopup("ms_ins_time");
    }
    if (ImGui::BeginPopup("ms_ins_time")) {
        timeMenu(st, view, timeHere);
        ImGui::EndPopup();
    }
    ribbon::endGroup(TR("Zeit"));

    ribbon::beginGroup();
    if (ribbon::big("##ins_break", icon::sceneBreak, TR("Szenenwechsel"),
                    TR("Eine Trennung mitten im Kapitel - im Export als * * * zentriert."))) {
        docops::insertOwnLine(doc, view, "---");
        done(view);
    }
    ImGui::SameLine();
    ImGui::BeginGroup();
    if (ribbon::labeled("##ins_bm", icon::bookmark, TR("Lesezeichen"), TR("Diese Stelle merken - erscheint im Navigationsbereich"))) {
        st.bookmarkDraft.clear();
        st.openBookmark = true;
    }
    if (ribbon::labeled("##ins_note", icon::comment, TR("Kommentar"), TR("Eine Notiz an diese Stelle haengen - steht nie im fertigen Text"))) {
        st.noteDraft.clear();
        st.noteBegin = st.noteEnd = kNone;
        st.openNote = true;
    }
    ImGui::EndGroup();
    ribbon::endGroup(TR("Text"));

    if (st.readMode) ImGui::EndDisabled();
}

void ribbonLayout(ManuscriptState& st, DocumentView& view) {
    AppSettings& s = theme::settings();
    bool changed = false;

    ribbon::beginGroup();
    if (ribbon::big("##lay_margins", icon::margins, TR("Seitenraender"), TR("Abstand des Textes zum Blattrand"), false, true))
        ImGui::OpenPopup("ms_margins");
    if (ImGui::BeginPopup("ms_margins")) {
        struct M {
            const char* name;
            float cm;
        };
        const M ms[] = {{"Schmal (1,27 cm)", 1.27f}, {"Mittel (1,9 cm)", 1.9f}, {"Normal (2,5 cm)", 2.5f},
                        {"Breit (3,5 cm)", 3.5f}};
        for (const M& m : ms) {
            const bool same = std::fabs(s.docMarginCm - m.cm) < 0.01f && std::fabs(s.docMarginLeftCm - m.cm) < 0.01f &&
                              std::fabs(s.docMarginRightCm - m.cm) < 0.01f;
            if (ImGui::Selectable(TR(m.name), same)) {
                s.docMarginCm = s.docMarginLeftCm = s.docMarginRightCm = m.cm;
                changed = true;
            }
        }
        ImGui::Separator();
        ImGui::SetNextItemWidth(140.0f);
        ImGui::TextUnformatted(TR("Oben / unten"));
        ImGui::SetNextItemWidth(140.0f);
        if (ImGui::SliderFloat("##margin", &s.docMarginCm, 0.5f, 6.0f, "%.2f cm")) changed = true;
        ImGui::TextUnformatted(TR("Links"));
        ImGui::SetNextItemWidth(140.0f);
        if (ImGui::SliderFloat("##marginl", &s.docMarginLeftCm, 0.0f, 6.0f, "%.2f cm")) changed = true;
        ImGui::TextUnformatted(TR("Rechts"));
        ImGui::SetNextItemWidth(140.0f);
        if (ImGui::SliderFloat("##marginr", &s.docMarginRightCm, 0.0f, 6.0f, "%.2f cm")) changed = true;
        ui::textSecondary(TR("Links und rechts auch direkt im Lineal ziehen."));
        ImGui::EndPopup();
    }
    ImGui::SameLine();
    if (ribbon::big("##lay_size", icon::pageSize, TR("Format"), TR("Papierformat"), false, true))
        ImGui::OpenPopup("ms_pagesize");
    if (ImGui::BeginPopup("ms_pagesize")) {
        for (int f = 0; f < 3; ++f) {
            float w = 0, h = 0;
            theme::pageSizeCm(f, &w, &h);
            char label[64];
            std::snprintf(label, sizeof(label), "%s  (%.1f x %.1f cm)", theme::pageFormatName(f), w, h);
            if (ImGui::Selectable(label, s.docPageFormat == f)) {
                s.docPageFormat = f;
                changed = true;
            }
        }
        ImGui::EndPopup();
    }
    ribbon::endGroup(TR("Seite einrichten"));

    ribbon::beginGroup();
    if (ribbon::big("##lay_spacing", icon::lineSpacing, TR("Zeilenabstand"), TR("Abstand zwischen den Zeilen"), false, true))
        ImGui::OpenPopup("ms_spacing2");
    if (ImGui::BeginPopup("ms_spacing2")) {
        for (float v : {1.0f, 1.15f, 1.5f, 2.0f, 2.5f, 3.0f}) {
            char label[16];
            std::snprintf(label, sizeof(label), "%.2g", v);
            if (ImGui::Selectable(label, std::fabs(s.docLineSpacing - v) < 0.01f)) {
                s.docLineSpacing = v;
                changed = true;
            }
        }
        ImGui::EndPopup();
    }
    ribbon::endGroup(TR("Absatz"));

    ribbon::beginGroup();
    {
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(TR("Grundschrift"));
        ImGui::SameLine();
        ImGui::SetNextItemWidth(ImGui::GetFontSize() * 9.0f);
        if (ImGui::BeginCombo("##docfont", s.docFont.c_str(), ImGuiComboFlags_HeightLarge)) {
            for (const std::string& name : theme::fontFamilies()) {
                ImGui::PushFont(theme::familyFont(name, false, false), 0.0f);
                if (ImGui::Selectable(name.c_str(), name == s.docFont)) {
                    s.docFont = name;
                    changed = true;
                }
                ImGui::PopFont();
            }
            ImGui::EndCombo();
        }
        ui::tooltip(TR("Schrift fuer den ganzen Text - auch im Word-Export"));
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(TR("Groesse"));
        ImGui::SameLine();
        ImGui::SetNextItemWidth(ImGui::GetFontSize() * 6.0f);
        if (ImGui::DragFloat("##docsize", &s.docFontSize, 0.25f, 6.0f, 72.0f, "%.1f pt")) changed = true;
    }
    ribbon::endGroup(TR("Dokument"));

    ribbon::beginGroup();
    if (ribbon::big("##lay_pages", icon::pageView, TR("Seiten"), TR("Einzelne Seiten wie auf Papier"), s.docPageView)) {
        s.docPageView = true;
        changed = true;
    }
    ImGui::SameLine();
    if (ribbon::big("##lay_web", icon::webView, TR("Endlos"), TR("Ein durchgehendes Blatt ueber die ganze Breite"),
                    !s.docPageView)) {
        s.docPageView = false;
        changed = true;
    }
    ribbon::endGroup(TR("Ansicht"));

    if (changed) {
        theme::save();
        done(view);
    }
    (void)st;
}

// Kommentare der Reihe nach anspringen.
void jumpToNote(ManuscriptState& st, DocumentView& view, int dir) {
    const std::vector<ManuscriptToken>& toks = st.doc.tokens();
    const size_t pos = dir > 0 ? view.selEnd() : view.selBegin();
    const ManuscriptToken* pick = nullptr;
    for (const ManuscriptToken& t : toks) {
        if (t.kind != ManuscriptToken::Kind::Note) continue;
        if (dir > 0 && t.begin >= pos) {
            pick = &t;
            break;
        }
        if (dir < 0 && t.end <= pos) pick = &t;
    }
    if (!pick) {
        // am Ende wieder von vorn
        for (const ManuscriptToken& t : toks) {
            if (t.kind != ManuscriptToken::Kind::Note) continue;
            pick = &t;
            if (dir > 0) break;
        }
    }
    if (pick) {
        view.select(pick->begin, pick->end);
        done(view);
    } else {
        st.doc.editor().setStatus(TR("Keine Kommentare im Text."));
    }
}

void ribbonReview(ManuscriptState& st, DocumentView& view) {
    ribbon::beginGroup();
    if (st.readMode) ImGui::BeginDisabled();
    if (ribbon::big("##rev_note", icon::comment, TR("Neuer Kommentar"), TR("Eine Notiz an diese Stelle haengen"))) {
        st.noteDraft.clear();
        st.noteBegin = st.noteEnd = kNone;
        st.openNote = true;
    }
    if (st.readMode) ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginGroup();
    if (ribbon::labeled("##rev_prev", icon::prev, TR("Vorheriger"), TR("Zum vorherigen Kommentar"))) jumpToNote(st, view, -1);
    if (ribbon::labeled("##rev_next", icon::next, TR("Naechster"), TR("Zum naechsten Kommentar"))) jumpToNote(st, view, 1);
    ImGui::EndGroup();
    ImGui::SameLine();
    // Loeschen, wenn genau ein Kommentar markiert ist (z.B. nach "Naechster")
    const ManuscriptToken* selectedNote = nullptr;
    for (const ManuscriptToken& t : st.doc.tokens()) {
        if (t.kind == ManuscriptToken::Kind::Note && t.begin == view.selBegin() && t.end == view.selEnd())
            selectedNote = &t;
    }
    ImGui::BeginGroup();
    if (ribbon::labeled("##rev_edit", icon::comment, TR("Bearbeiten"), TR("Markierten Kommentar bearbeiten"), false,
                        selectedNote != nullptr && !st.readMode)) {
        st.noteDraft = selectedNote->field;
        st.noteBegin = selectedNote->begin;
        st.noteEnd = selectedNote->end;
        st.openNote = true;
    }
    if (ribbon::labeled("##rev_del", icon::trash, TR("Loeschen"), TR("Markierten Kommentar loeschen"), false,
                        selectedNote != nullptr && !st.readMode)) {
        docops::replaceRaw(st.doc, view, selectedNote->begin, selectedNote->end, "");
        done(view);
    }
    ImGui::EndGroup();
    ribbon::endGroup(TR("Kommentare"));

    ribbon::beginGroup();
    if (st.readMode) ImGui::BeginDisabled();
    if (ribbon::big("##rev_bm", icon::bookmark, TR("Lesezeichen"), TR("Diese Stelle merken"))) {
        st.bookmarkDraft.clear();
        st.openBookmark = true;
    }
    if (st.readMode) ImGui::EndDisabled();
    ImGui::SameLine();
    if (ribbon::big("##rev_goto", icon::next, TR("Gehe zu"), TR("Zu einem Lesezeichen springen"), false, true))
        ImGui::OpenPopup("ms_goto");
    if (ImGui::BeginPopup("ms_goto")) {
        bool any = false;
        for (const ManuscriptToken& t : st.doc.tokens()) {
            if (t.kind != ManuscriptToken::Kind::Bookmark) continue;
            any = true;
            ImGui::PushID(static_cast<int>(t.begin));
            if (ImGui::Selectable(t.field.empty() ? TR("(ohne Namen)") : t.field.c_str())) {
                view.setCaret(t.begin);
                done(view);
            }
            ImGui::PopID();
        }
        if (!any) ui::textSecondary(TR("Noch keine Lesezeichen."));
        ImGui::EndPopup();
    }
    ribbon::endGroup(TR("Lesezeichen"));

    ribbon::beginGroup();
    if (ribbon::big("##rev_count", icon::count, TR("Woerter zaehlen"), TR("Seiten, Woerter, Zeichen und Absaetze")))
        st.openWordCount = true;
    ribbon::endGroup(TR("Dokumentpruefung"));

    ribbon::beginGroup();
    {
        AppSettings& s = theme::settings();
        if (ribbon::big("##rev_spell", icon::spelling, TR("Rechtschreibung"),
                        TR("Unbekannte Woerter rot unterwellen - Rechtsklick zeigt Vorschlaege"), s.docSpellCheck)) {
            s.docSpellCheck = !s.docSpellCheck;
            theme::save();
        }
        ImGui::SameLine();
        ImGui::BeginGroup();
        if (ribbon::labeled("##rev_auto", icon::autoCorrect, TR("AutoKorrektur"),
                            TR("Beim Tippen: Satzanfang gross, typografische Anfuehrungszeichen, ... und "
                               "Gedankenstriche, bekannte Tippfehler. Strg+Z direkt danach nimmt es zurueck."),
                            s.docAutoCorrect)) {
            s.docAutoCorrect = !s.docAutoCorrect;
            theme::save();
        }
        const std::string lang = languageName(resolveLanguage(s.docLanguage));
        if (ribbon::labeled("##rev_lang", icon::webView, lang.c_str(), TR("Sprache des Textes"))) ImGui::OpenPopup("ms_lang");
        if (ImGui::BeginPopup("ms_lang")) {
            languageMenu();
            ImGui::EndPopup();
        }
        ImGui::EndGroup();
        ImGui::SameLine();
        if (ribbon::labeled("##rev_next_err", icon::next, TR("Naechster Fehler"), TR("Zum naechsten unbekannten Wort (F7)"),
                            false, s.docSpellCheck && spell::available())) {
            size_t b = 0, e = 0;
            if (docops::findSpellingError(st.doc, view.selEnd(), &b, &e)) {
                view.select(b, e);
                done(view);
            } else {
                st.doc.editor().setStatus(TR("Keine Rechtschreibfehler gefunden."));
            }
        }
    }
    ribbon::endGroup(TR("Rechtschreibung"));
}

void ribbonView(ManuscriptState& st, DocumentView& view, const DocOptions& opt) {
    AppSettings& s = theme::settings();
    bool changed = false;

    ribbon::beginGroup();
    if (ribbon::big("##v_read", icon::readMode, TR("Lesemodus"),
                    TR("So liest sich die Geschichte fertig - alle Werte sind eingesetzt."), st.readMode))
        st.readMode = !st.readMode;
    ImGui::SameLine();
    if (ribbon::big("##v_page", icon::pageView, TR("Seitenlayout"), TR("Schreiben auf Seiten wie auf Papier"),
                    !st.readMode && s.docPageView)) {
        st.readMode = false;
        s.docPageView = true;
        changed = true;
    }
    ImGui::SameLine();
    if (ribbon::big("##v_web", icon::webView, TR("Endlos"), TR("Ein durchgehendes Blatt ueber die ganze Breite"),
                    !st.readMode && !s.docPageView)) {
        st.readMode = false;
        s.docPageView = false;
        changed = true;
    }
    ribbon::endGroup(TR("Ansichten"));

    ribbon::beginGroup();
    ImGui::BeginGroup();
    if (ribbon::labeled("##v_ruler", icon::ruler, TR("Lineal"), TR("Lineal ueber der Seite"), s.docShowRuler)) {
        s.docShowRuler = !s.docShowRuler;
        changed = true;
    }
    if (ribbon::labeled("##v_nav", icon::outline, TR("Navigationsbereich"), TR("Kapitel, Szenen, Zeitpunkte und Lesezeichen links"),
                        s.docShowOutline)) {
        s.docShowOutline = !s.docShowOutline;
        changed = true;
    }
    ImGui::EndGroup();
    ImGui::SameLine();
    if (ribbon::labeled("##v_marks", icon::marks, TR("Marken"), TR("Zeitpunkte und Aktionen auch im Lesemodus zeigen"),
                        s.docShowMarks)) {
        s.docShowMarks = !s.docShowMarks;
        changed = true;
    }
    ribbon::endGroup(TR("Anzeigen"));

    ribbon::beginGroup();
    if (ribbon::big("##v_zoom", icon::zoom, TR("Zoom"), TR("Vergroesserung waehlen (Strg+Mausrad)"), false, true))
        ImGui::OpenPopup("ms_zoom");
    if (ImGui::BeginPopup("ms_zoom")) {
        for (int pct : {50, 75, 100, 125, 150, 200, 300}) {
            char label[16];
            std::snprintf(label, sizeof(label), "%d %%", pct);
            if (ImGui::Selectable(label, std::fabs(s.docZoom * 100.0f - static_cast<float>(pct)) < 0.5f)) {
                s.docZoom = static_cast<float>(pct) / 100.0f;
                changed = true;
            }
        }
        ImGui::EndPopup();
    }
    ImGui::SameLine();
    ImGui::BeginGroup();
    if (ribbon::labeled("##v_100", icon::zoom, "100 %", TR("Originalgroesse"))) {
        s.docZoom = 1.0f;
        changed = true;
    }
    if (ribbon::labeled("##v_one", icon::onePage, TR("Eine Seite"), TR("Die ganze Seite sichtbar"))) {
        s.docZoom = view.zoomForWholePage(opt);
        s.docPageView = true;
        changed = true;
    }
    ImGui::EndGroup();
    ImGui::SameLine();
    if (ribbon::labeled("##v_width", icon::pageWidth, TR("Seitenbreite"), TR("Seite so breit wie das Fenster"))) {
        s.docZoom = view.zoomForPageWidth(opt);
        changed = true;
    }
    ribbon::endGroup(TR("Zoom"));

    ribbon::beginGroup();
    if (ribbon::big("##v_split", icon::split, TR("Teilen"), TR("Zwei Stellen desselben Textes uebereinander"), st.split)) {
        st.split = !st.split;
        if (st.split) {
            st.second.cursor = st.main.cursor;
            st.second.anchor = st.main.cursor;
            st.second.requestScrollToCaret();
        }
        st.main.activeView = true;
        st.second.activeView = false;
    }
    ImGui::SameLine();
    if (ribbon::big("##v_focus", icon::focus, TR("Fokus"), TR("Nur der Text - Menueband und Navigationsbereich verschwinden"),
                    st.focusMode))
        st.focusMode = !st.focusMode;
    ribbon::endGroup(TR("Fenster"));

    if (changed) {
        theme::save();
        done(view);
    }
}

void ribbonFile(Editor& ed, ManuscriptState& st) {
    ribbon::beginGroup();
    if (ribbon::big("##f_save", icon::save, TR("Speichern"), TR("Alles in den Vault schreiben (Strg+S)")))
        ed.saveEverything();
    ImGui::SameLine();
    if (ribbon::big("##f_word", icon::word, TR("Als Word"),
                    TR("Schreibt die fertige Geschichte als .docx: Werte sind eingesetzt, Formatierung "
                       "bleibt, Marken, Lesezeichen und Kommentare stehen nicht darin.")))
        ed.exportManuscriptToWord();
    ribbon::endGroup(TR("Datei"));

    ribbon::beginGroup();
    if (ribbon::big("##f_help", icon::help, TR("Hilfe"), TR("Kurz erklaert, wie dieses Fenster funktioniert.")))
        st.showHelp = true;
    ImGui::SameLine();
    if (ribbon::big("##f_settings", icon::settings, TR("Einstellungen"), TR("Farben, Schriftgroesse der Oberflaeche, Speichern")))
        theme::settings().showSettings = true;
    ribbon::endGroup(TR("Programm"));
}

void drawRibbon(Editor& ed, ManuscriptState& st, DocumentView& view, const DocOptions& opt, long long timeHere) {
    const ColorScheme& c = theme::colors();
    AppSettings& s = theme::settings();
    ManuscriptDoc& doc = st.doc;

    // Schnellzugriff + Registerkarten
    if (ribbon::tool("##q_save", icon::save, TR("Speichern (Strg+S)"))) ed.saveEverything();
    ImGui::SameLine(0.0f, 2.0f);
    const bool canUndo = doc.canUndo() || ed.canUndo();
    if (ribbon::tool("##q_undo", icon::undo, TR("Rueckgaengig (Strg+Z)"), false, canUndo && !st.readMode)) {
        if (!doc.undo(&view)) ed.undo();
        done(view);
    }
    ImGui::SameLine(0.0f, 2.0f);
    const bool canRedo = doc.canRedo() || ed.canRedo();
    if (ribbon::tool("##q_redo", icon::redo, TR("Wiederholen (Strg+Y)"), false, canRedo && !st.readMode)) {
        if (!doc.redo(&view)) ed.redo();
        done(view);
    }
    ImGui::SameLine();
    ui::verticalSeparator();
    const char* tabs[TabCount] = {TR("Datei"), TR("Start"), TR("Einfuegen"), TR("Layout"), TR("Ueberpruefen"),
                                  TR("Ansicht")};
    if (ribbon::tabStrip(&s.docRibbonTab, tabs, TabCount)) {
        s.docRibbonCollapsed = false;
        theme::save();
    }
    // Band ein-/ausklappen, rechts in der Reihe
    ImGui::SameLine();
    const float right = ImGui::GetContentRegionAvail().x - ribbon::rowHeight();
    if (right > 0.0f) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + right);
    if (ribbon::tool("##collapse", s.docRibbonCollapsed ? icon::chevronDown : icon::chevronUp,
                     s.docRibbonCollapsed ? TR("Menueband anzeigen") : TR("Menueband ausblenden"))) {
        s.docRibbonCollapsed = !s.docRibbonCollapsed;
        theme::save();
    }
    if (s.docRibbonCollapsed) return;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, c.panelBackground);
    // Ist das Fenster schmaler als das Band, kommt eine Scrollleiste dazu -
    // ihr Platz wird nur dann reserviert (Stand des letzten Frames).
    static bool needsScrollbar = false;
    const float bodyH = ribbon::bodyHeight() + ImGui::GetStyle().WindowPadding.y * 2.0f +
                        (needsScrollbar ? ImGui::GetStyle().ScrollbarSize : 0.0f);
    ImGui::BeginChild("##ribbon", ImVec2(0, bodyH), ImGuiChildFlags_Borders,
                      ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                          ImGuiWindowFlags_NoNavInputs);
    needsScrollbar = ImGui::GetScrollMaxX() > 0.0f;
    ribbon::beginBody();
    switch (s.docRibbonTab) {
        case TabFile: ribbonFile(ed, st); break;
        case TabHome: ribbonHome(ed, st, view, opt); break;
        case TabInsert: ribbonInsert(ed, st, view, timeHere); break;
        case TabLayout: ribbonLayout(st, view); break;
        case TabReview: ribbonReview(st, view); break;
        case TabView: ribbonView(st, view, opt); break;
        default: break;
    }
    ribbon::endBody();
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ------------------------------------------------------- Navigationsbereich
void drawNavigation(ManuscriptState& st, DocumentView& view, float height) {
    const ColorScheme& c = theme::colors();
    ImGui::BeginChild("##navigation", ImVec2(ImGui::GetFontSize() * 13.0f, height), ImGuiChildFlags_Borders);
    ImGui::TextUnformatted(TR("Navigation"));
    ui::tooltip(TR("Klick springt an die Stelle im Text."));
    ImGui::Separator();
    bool any = false;
    int notes = 0;
    for (const ManuscriptToken& t : st.doc.tokens()) {
        ImGui::PushID(static_cast<int>(t.begin));
        if (t.kind == ManuscriptToken::Kind::Heading) {
            any = true;
            const size_t hashes = t.raw.find_first_not_of('#');
            const int level = hashes == std::string::npos ? 1 : static_cast<int>(hashes);
            const size_t firstText = t.raw.find_first_not_of("# ");
            const std::string title = firstText == std::string::npos ? std::string(TR("(leer)")) : t.raw.substr(firstText);
            const float indent = static_cast<float>(std::max(0, level - 1)) * 10.0f;
            if (indent > 0.0f) ImGui::Indent(indent);
            if (level <= 2) ImGui::PushFont(theme::fontFor(true, false), 0.0f);
            if (ImGui::Selectable(title.c_str())) {
                view.setCaret(t.begin);
                done(view);
            }
            if (level <= 2) ImGui::PopFont();
            if (indent > 0.0f) ImGui::Unindent(indent);
        } else if (t.kind == ManuscriptToken::Kind::Time) {
            any = true;
            ImGui::Indent(10.0f);
            ImGui::PushStyleColor(ImGuiCol_Text, c.textSecondary);
            if (ImGui::Selectable(formatStoryTime(t.time).c_str())) {
                view.setCaret(t.begin);
                done(view);
            }
            ImGui::PopStyleColor();
            ImGui::Unindent(10.0f);
        } else if (t.kind == ManuscriptToken::Kind::Bookmark) {
            any = true;
            ImGui::Indent(10.0f);
            ImGui::PushStyleColor(ImGuiCol_Text, c.accentColor);
            const std::string label = std::string("\xE2\x96\xB8 ") + (t.field.empty() ? TR("Lesezeichen") : t.field);
            if (ImGui::Selectable(label.c_str())) {
                view.setCaret(t.begin);
                done(view);
            }
            ImGui::PopStyleColor();
            ImGui::Unindent(10.0f);
        } else if (t.kind == ManuscriptToken::Kind::Note) {
            ++notes;
        }
        ImGui::PopID();
    }
    if (!any)
        ui::textSecondary(TR("Kapitel, Szenen, Zeitpunkte und Lesezeichen erscheinen hier."));

    if (notes > 0) {
        ImGui::Spacing();
        if (ImGui::CollapsingHeader((std::string(TR("Kommentare")) + " (" + std::to_string(notes) + ")###notes").c_str())) {
            for (const ManuscriptToken& t : st.doc.tokens()) {
                if (t.kind != ManuscriptToken::Kind::Note) continue;
                ImGui::PushID(static_cast<int>(t.begin));
                if (ImGui::Selectable(ui::ellipsis(t.field, 40).c_str())) {
                    view.select(t.begin, t.end);
                    done(view);
                }
                ui::tooltip(t.field.c_str());
                ImGui::PopID();
            }
        }
    }
    ImGui::EndChild();
}

// ------------------------------------------------------------ Statusleiste
void drawStatusBar(Editor& ed, ManuscriptState& st, DocumentView& view, const DocOptions& opt, long long timeHere) {
    const ColorScheme& c = theme::colors();
    AppSettings& s = theme::settings();
    ManuscriptDoc& doc = st.doc;

    // Zaehlen heisst: den ganzen Text einmal setzen - beim Tippen reicht das
    // zweimal pro Sekunde.
    static double countedAt = -10.0;
    if (st.wordsGeneration != doc.generation() && ImGui::GetTime() - countedAt > 0.5) {
        st.words = countWords(renderManuscript(ed.project, doc.text()));
        st.wordsGeneration = doc.generation();
        countedAt = ImGui::GetTime();
    }

    ImGui::PushStyleColor(ImGuiCol_ChildBg, theme::mix(c.panelBackground, c.backgroundColor, 0.5f));
    ImGui::BeginChild("##status", ImVec2(0, ImGui::GetFrameHeight() + 4.0f), ImGuiChildFlags_None,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2.0f);

    char buf[128];
    std::snprintf(buf, sizeof(buf), TR("Seite %d von %d"), view.caretPage() + 1, std::max(1, view.pageCount()));
    ImGui::AlignTextToFramePadding();
    ui::textSecondary(opt.pageView ? buf : TR("Endlos"));
    ImGui::SameLine(0.0f, 18.0f);
    if (view.hasSelection()) {
        const size_t selWords = countWords(docops::selectedPlainText(doc, view));
        std::snprintf(buf, sizeof(buf), TR("%zu von %zu Woertern"), selWords, st.words);
    } else {
        std::snprintf(buf, sizeof(buf), TR("%zu Woerter"), st.words);
    }
    if (ImGui::SmallButton(buf)) st.openWordCount = true;
    ui::tooltip(TR("Woerter zaehlen"));

    ImGui::SameLine(0.0f, 18.0f);
    // Der Knopf ist zugleich die Anzeige: man sieht immer, welcher Zeitpunkt
    // an der Cursorstelle gilt. Ohne das merkt niemand, dass es ihn gibt.
    ImGui::PushStyleColor(ImGuiCol_Text, c.accentColor);
    const std::string timeLabel = TR("Zeit: ") + formatStoryTime(timeHere) + "###mstime";
    const bool timeClicked = ImGui::SmallButton(timeLabel.c_str());
    ImGui::PopStyleColor();
    if (timeClicked && !st.readMode) {
        st.timeDraft = formatStoryTime(timeHere);
        ImGui::OpenPopup("ms_time");
    }
    ui::tooltip(TR("Der Zeitpunkt, an dem die Geschichte an dieser Stelle steht. Klicken, um "
                   "die Zeit ab hier weiterzustellen."));
    if (ImGui::BeginPopup("ms_time")) {
        timeMenu(st, view, timeHere);
        ImGui::EndPopup();
    }

    ImGui::SameLine(0.0f, 18.0f);
    {
        const std::string tag = resolveLanguage(s.docLanguage);
        const bool ok = languageInstalled(tag);
        const std::string label = languageName(tag) + "###mslang";
        if (!ok) ImGui::PushStyleColor(ImGuiCol_Text, c.warningColor);
        if (ImGui::SmallButton(label.c_str())) ImGui::OpenPopup("ms_lang_status");
        if (!ok) ImGui::PopStyleColor();
        ui::tooltip(ok ? TR("Sprache des Textes - fuer Rechtschreibung und AutoKorrektur")
                       : TR("Diese Sprache ist in Windows nicht installiert."));
        if (ImGui::BeginPopup("ms_lang_status")) {
            languageMenu();
            ImGui::Separator();
            if (ImGui::MenuItem(TR("Rechtschreibung pruefen"), nullptr, s.docSpellCheck)) {
                s.docSpellCheck = !s.docSpellCheck;
                theme::save();
            }
            if (ImGui::MenuItem(TR("AutoKorrektur"), nullptr, s.docAutoCorrect)) {
                s.docAutoCorrect = !s.docAutoCorrect;
                theme::save();
            }
            ImGui::EndPopup();
        }
    }

    ImGui::SameLine(0.0f, 18.0f);
    const bool unsaved = ed.hasUnsavedChanges();
    ui::colorDot(unsaved ? c.warningColor : c.successColor);
    ImGui::SameLine();
    if (unsaved) {
        if (ImGui::SmallButton(TR("Speichern"))) ed.saveEverything();
        ui::tooltip(s.autosave ? TR("Wird gleich von selbst geschrieben - Klick speichert sofort.")
                               : TR("Automatisches Speichern ist aus. Klicken oder Strg+S."));
    } else {
        ui::textSecondary(TR("Gespeichert"));
        ui::tooltip(TR("Alles steht im Vault."));
    }

    // rechts: Fokus, Ansichten, Zoom
    const float zoomW = 110.0f;
    const float h = ImGui::GetFrameHeight();
    const float needed = h * 4.0f + zoomW + h * 2.0f + ImGui::CalcTextSize("300 %").x + 60.0f;
    ImGui::SameLine();
    const float slack = ImGui::GetContentRegionAvail().x - needed;
    if (slack > 0.0f) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + slack);
    ImGui::SetCursorPosY(2.0f);
    bool changed = false;
    if (ribbon::tool("##sb_focus", icon::focus, TR("Fokus"), st.focusMode)) st.focusMode = !st.focusMode;
    ImGui::SameLine(0.0f, 8.0f);
    if (ribbon::tool("##sb_read", icon::readMode, TR("Lesemodus"), st.readMode)) st.readMode = !st.readMode;
    ImGui::SameLine(0.0f, 2.0f);
    if (ribbon::tool("##sb_page", icon::pageView, TR("Seitenlayout"), !st.readMode && s.docPageView)) {
        st.readMode = false;
        s.docPageView = true;
        changed = true;
    }
    ImGui::SameLine(0.0f, 2.0f);
    if (ribbon::tool("##sb_web", icon::webView, TR("Endlos"), !st.readMode && !s.docPageView)) {
        st.readMode = false;
        s.docPageView = false;
        changed = true;
    }
    ImGui::SameLine(0.0f, 10.0f);
    if (ImGui::Button("-", ImVec2(h, h))) {
        s.docZoom = std::max(0.3f, std::round((s.docZoom - 0.1f) * 10.0f) / 10.0f);
        changed = true;
    }
    ImGui::SameLine(0.0f, 2.0f);
    ImGui::SetNextItemWidth(zoomW);
    float pct = s.docZoom * 100.0f;
    if (ImGui::SliderFloat("##zoom", &pct, 30.0f, 300.0f, "", ImGuiSliderFlags_AlwaysClamp)) {
        s.docZoom = pct / 100.0f;
        changed = true;
    }
    ui::tooltip(TR("Zoom (Strg+Mausrad)"));
    ImGui::SameLine(0.0f, 2.0f);
    if (ImGui::Button("+", ImVec2(h, h))) {
        s.docZoom = std::min(4.0f, std::round((s.docZoom + 0.1f) * 10.0f) / 10.0f);
        changed = true;
    }
    ImGui::SameLine();
    std::snprintf(buf, sizeof(buf), "%d %%", static_cast<int>(std::round(s.docZoom * 100.0f)));
    if (ImGui::SmallButton(buf)) {
        s.docZoom = 1.0f;
        changed = true;
    }
    ui::tooltip(TR("Klick: 100 %"));
    if (changed) theme::save();

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ------------------------------------------------- Dialoge im Fenster
void drawDialogs(Editor& ed, ManuscriptState& st, DocumentView& view) {
    ManuscriptDoc& doc = st.doc;
    if (st.openNote) {
        ImGui::OpenPopup("ms_note");
        st.openNote = false;
    }
    if (st.openBookmark) {
        ImGui::OpenPopup("ms_bookmark");
        st.openBookmark = false;
    }
    if (st.openWordCount) {
        ImGui::OpenPopup("ms_wordcount");
        st.openWordCount = false;
    }

    if (ImGui::BeginPopup("ms_note")) {
        ImGui::TextUnformatted(st.noteBegin == kNone ? TR("Neuer Kommentar") : TR("Kommentar"));
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        ImGui::InputTextMultiline("##notetext", &st.noteDraft, ImVec2(340, 110));
        const std::string clean = sanitizeMarkerText(trim(st.noteDraft));
        if (clean.empty()) ImGui::BeginDisabled();
        if (ImGui::Button(TR("Speichern"))) {
            const std::string marker = "%%note:" + clean + "%%";
            if (st.noteBegin != kNone && st.noteEnd <= doc.text().size()) {
                docops::replaceRaw(doc, view, st.noteBegin, st.noteEnd, marker);
            } else {
                view.setCaret(view.selEnd());
                docops::insertSource(doc, view, marker);
            }
            done(view);
            ImGui::CloseCurrentPopup();
        }
        if (clean.empty()) ImGui::EndDisabled();
        ImGui::SameLine();
        if (st.noteBegin != kNone) {
            if (ImGui::Button(TR("Loeschen"))) {
                docops::replaceRaw(doc, view, st.noteBegin, st.noteEnd, "");
                done(view);
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
        }
        if (ImGui::Button(TR("Abbrechen"))) {
            done(view);
            ImGui::CloseCurrentPopup();
        }
        ui::textSecondary(TR("Kommentare stehen nie im fertigen Text."));
        ImGui::EndPopup();
    }

    if (ImGui::BeginPopup("ms_bookmark")) {
        ImGui::TextUnformatted(TR("Lesezeichen"));
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        ImGui::SetNextItemWidth(240.0f);
        const bool entered = ImGui::InputTextWithHint("##bmname", TR("Name, z.B. \"Hier weiter\""),
                                                      &st.bookmarkDraft, ImGuiInputTextFlags_EnterReturnsTrue);
        const std::string clean = sanitizeMarkerText(trim(st.bookmarkDraft));
        if (clean.empty()) ImGui::BeginDisabled();
        if (ImGui::Button(TR("Setzen")) || (entered && !clean.empty())) {
            view.setCaret(view.selBegin());
            docops::insertSource(doc, view, "%%bm:" + clean + "%%");
            done(view);
            ImGui::CloseCurrentPopup();
        }
        if (clean.empty()) ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button(TR("Abbrechen"))) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    if (ImGui::BeginPopup("ms_wordcount")) {
        const std::string rendered = renderManuscript(ed.project, doc.text());
        size_t chars = 0, charsNoSpace = 0;
        for (size_t i = 0; i < rendered.size();) {
            const unsigned char ch = static_cast<unsigned char>(rendered[i]);
            const size_t n = ch < 0x80 ? 1 : (ch >> 5) == 6 ? 2 : (ch >> 4) == 14 ? 3 : 4;
            if (ch != '\n' && ch != '\r') {
                ++chars;
                if (ch != ' ' && ch != '\t') ++charsNoSpace;
            }
            i += n;
        }
        size_t paragraphs = 0;
        for (const ManuscriptLine& L : doc.lines()) {
            if ((L.kind == LineKind::Body || L.kind == LineKind::Bullet || L.kind == LineKind::Numbered) &&
                L.contentEnd > L.contentBegin)
                ++paragraphs;
        }
        size_t chapters = 0;
        for (const ManuscriptLine& L : doc.lines()) {
            if (L.kind == LineKind::Heading && L.level <= 2) ++chapters;
        }
        ImGui::TextUnformatted(TR("Statistik"));
        ImGui::Separator();
        if (ImGui::BeginTable("##stats", 2, ImGuiTableFlags_SizingFixedFit)) {
            auto row = [](const char* label, size_t value) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(label);
                ImGui::TableNextColumn();
                ImGui::Text("%zu", value);
            };
            row(TR("Seiten"), static_cast<size_t>(std::max(1, view.pageCount())));
            row(TR("Woerter"), countWords(rendered));
            row(TR("Zeichen (ohne Leerzeichen)"), charsNoSpace);
            row(TR("Zeichen (mit Leerzeichen)"), chars);
            row(TR("Absaetze"), paragraphs);
            row(TR("Kapitel"), chapters);
            ImGui::EndTable();
        }
        if (ImGui::Button(TR("Schliessen"))) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    // Rechtsklick auf die Seite
    if (st.openContext) {
        ImGui::OpenPopup("ms_context");
        st.openContext = false;
    }
    if (ImGui::BeginPopup("ms_context")) {
        const bool sel = view.hasSelection();
        const bool writing = !st.readMode;
        if (!st.spellWord.empty() && writing) {
            if (st.spellSuggestions.empty()) ui::textSecondary(TR("(keine Vorschlaege)"));
            for (const std::string& suggestion : st.spellSuggestions) {
                ImGui::PushFont(theme::fontFor(true, false), 0.0f);
                if (ImGui::MenuItem(suggestion.c_str())) {
                    docops::replaceRaw(doc, view, st.spellBegin, st.spellEnd, suggestion);
                    done(view);
                }
                ImGui::PopFont();
            }
            ImGui::Separator();
            if (ImGui::MenuItem(TR("Alle ignorieren"))) spell::ignoreForSession(st.spellWord);
            ui::tooltip(TR("Bis zum Neustart nicht mehr markieren"));
            if (ImGui::MenuItem(TR("Zum Woerterbuch hinzufuegen"))) spell::addToDictionary(st.spellWord);
            ui::tooltip(TR("Gilt dauerhaft als richtig (eigene Liste des Programms)"));
            ImGui::Separator();
        }
        if (ImGui::MenuItem(TR("Ausschneiden"), TR("Strg+X"), false, sel && writing)) docops::cut(doc, view);
        if (ImGui::MenuItem(TR("Kopieren"), TR("Strg+C"), false, sel)) docops::copy(doc, view);
        if (ImGui::MenuItem(TR("Einfuegen"), TR("Strg+V"), false, writing)) docops::paste(doc, view);
        ImGui::Separator();
        if (ImGui::MenuItem(TR("Fett"), TR("Strg+B"), styleActive(doc, view, &TextStyle::bold), writing))
            toggleStyle(doc, view, &TextStyle::bold);
        if (ImGui::MenuItem(TR("Kursiv"), TR("Strg+I"), styleActive(doc, view, &TextStyle::italic), writing))
            toggleStyle(doc, view, &TextStyle::italic);
        if (ImGui::MenuItem(TR("Unterstrichen"), TR("Strg+U"), styleActive(doc, view, &TextStyle::underline), writing))
            toggleStyle(doc, view, &TextStyle::underline);
        ImGui::Separator();
        const long long timeHere = timeAt(doc, view.cursor);
        if (ImGui::BeginMenu(TR("Element einfuegen"), writing)) {
            elementPickerMenu(ed, st, view, false, timeHere);
            ImGui::EndMenu();
        }
        if (ImGui::MenuItem(TR("Kommentar hinzufuegen"), nullptr, false, writing)) {
            st.noteDraft.clear();
            st.noteBegin = st.noteEnd = kNone;
            st.openNote = true;
        }
        if (ImGui::MenuItem(TR("Lesezeichen setzen"), nullptr, false, writing)) {
            st.bookmarkDraft.clear();
            st.openBookmark = true;
        }
        if (ImGui::MenuItem(TR("Aktion aus diesem Absatz"), nullptr, false, writing))
            actionFromParagraph(ed, st, view);
        ImGui::EndPopup();
    }
}

// ------------------------------------------------ Autovervollstaendigung
// "@Ali" -> Elemente, "@Alice." -> Felder, "!Tit" -> Aktionen. Die Liste steht
// direkt unter dem Cursor.
void updateCompletion(Editor& ed, ManuscriptState& st, DocumentView& view, long long timeHere) {
    const ColorScheme& c = theme::colors();
    ManuscriptDoc& doc = st.doc;
    const std::string& text = doc.text();

    const std::string previousWord = st.completionWord;
    st.completionStart = kNone;
    st.completionSigil = 0;
    st.completionWord.clear();
    if (view.focused() && !view.hasSelection() && !st.readMode) {
        // Rueckwaerts bis zum "@" bzw. "!" - Elementnamen enthalten keine
        // Leerzeichen, Aktionstitel schon.
        const size_t cursor = std::min(view.cursor, text.size());
        size_t i = cursor;
        bool space = false;
        while (i > 0) {
            const char ch = text[i - 1];
            if (ch == '\n' || ch == '\t') break;
            if (ch == '@') {
                if (!space) {
                    st.completionStart = i - 1;
                    st.completionSigil = ch;
                }
                break;
            }
            if (ch == '!') {
                // "!" nur am Wortanfang und direkt vor dem Titel - sonst waere
                // jedes "Hallo! Wie" eine Aktionssuche.
                const bool wordStart = i < 2 || text[i - 2] == ' ' || text[i - 2] == '\n';
                const bool glued = i < cursor && text[i] != ' ';
                if (cursor - i <= 40 && wordStart && (glued || i == cursor)) {
                    st.completionStart = i - 1;
                    st.completionSigil = ch;
                }
                break;
            }
            if (ch == ' ') space = true;
            --i;
        }
        if (st.completionStart != kNone) st.completionWord = text.substr(st.completionStart + 1, cursor - st.completionStart - 1);
        if (st.completionSigil == '!' && st.completionWord.rfind("act:", 0) == 0) st.completionStart = kNone;
    }
    if (st.completionWord != previousWord) {
        st.completionPick = 0;
        st.pickMoved = false;
        if (st.muted && st.completionWord != st.mutedWord) st.muted = false;
    }
    // Wer nur hineinklickt oder mit den Pfeilen hinwandert, hat nichts
    // getippt: dann bleibt die Liste zu, bis sich das Wort aendert.
    const bool typed = doc.generation() != st.completionGeneration;
    const bool moved = view.cursor != st.completionCursor;
    st.completionGeneration = doc.generation();
    st.completionCursor = view.cursor;
    if (moved && !typed && !st.completionOpen) {
        st.muted = true;
        st.mutedWord = st.completionWord;
    }

    st.completionOpen = false;
    if (st.completionStart == kNone || st.muted || !view.caretVisible()) {
        st.pickText.clear();
        return;
    }

    const size_t dot = st.completionWord.find('.');
    std::vector<std::string> matches;
    std::vector<std::string> labels;
    std::vector<ImVec4> colors;
    bool fieldMode = false;
    const bool actionMode = st.completionSigil == '!';
    bool filtered = !st.completionWord.empty();

    if (actionMode) {
        for (const Action* a : ed.project.sortedActions()) {
            if (!st.completionWord.empty() && !iequalsContains(a->title, st.completionWord)) continue;
            matches.push_back("act:" + a->id);
            labels.push_back(formatStoryTime(ed.project.resolveActionTime(*a)) + "  " + a->title);
            colors.push_back(c.warningColor);
            if (matches.size() >= 8) break;
        }
    } else if (dot != std::string::npos) {
        const std::string elementPart = st.completionWord.substr(0, dot);
        const std::string fieldPart = st.completionWord.substr(dot + 1);
        const Element* owner = nullptr;
        for (const Element& el : ed.project.elements) {
            if (el.name == elementPart || ed.project.elementPath(el.id) == elementPart) owner = &el;
        }
        if (owner) {
            fieldMode = true;
            filtered = !fieldPart.empty();
            for (const std::string& key : owner->fieldOrder) {
                if (!fieldPart.empty() && !iequalsContains(key, fieldPart)) continue;
                const std::string value = ed.project.valueAt(owner->id, key, timeHere);
                matches.push_back(elementPart + "." + key);
                labels.push_back(key + (value.empty() ? std::string(" ") + TR("(leer)")
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
    if (matches.empty()) {
        st.pickText.clear();
        return;
    }

    const int count = static_cast<int>(matches.size());
    if (st.completionPick < 0) st.completionPick = count - 1;
    if (st.completionPick >= count) st.completionPick = 0;
    st.pickText = matches[static_cast<size_t>(st.completionPick)];
    st.pickFiltered = filtered;
    st.completionOpen = true;

    const ImVec2 caret = view.caretScreenPos();
    ImGui::SetNextWindowPos(ImVec2(caret.x, caret.y + view.caretHeight() + 3.0f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.97f);
    ImGui::Begin("##completion", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize |
                     ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
    ui::textSecondary(actionMode  ? TR("Aktion waehlen - Enter uebernimmt")
                      : fieldMode ? TR("Feld waehlen - Enter uebernimmt")
                                  : TR("Enter uebernimmt, Pfeile waehlen, Esc schliesst"));
    for (size_t i = 0; i < matches.size(); ++i) {
        ui::colorDot(colors[i]);
        const bool picked = static_cast<int>(i) == st.completionPick;
        if (picked) ImGui::PushStyleColor(ImGuiCol_Text, c.accentColor);
        ImGui::TextUnformatted(labels[i].c_str());
        if (picked) ImGui::PopStyleColor();
        if (picked) {
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, c.accentColor);
            ImGui::TextUnformatted("<");
            ImGui::PopStyleColor();
        }
    }
    ImGui::End();
}

// Tasten fuer die offene Vorschlagsliste - vor dem Schreibfeld ausgewertet.
void completionKeys(ManuscriptState& st, DocumentView& view) {
    if (!st.completionOpen || !view.focused()) return;
    const bool enterAccepts = st.pickFiltered || st.pickMoved;
    const bool byEnter = ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false);
    const bool accept = ImGui::IsKeyPressed(ImGuiKey_Tab, false) || (byEnter && enterAccepts);
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
    if (accept && !st.pickText.empty() && st.completionStart != kNone) {
        const size_t from = st.completionStart + 1;
        if (from <= view.cursor) {
            docops::replaceRaw(st.doc, view, from, view.cursor, st.pickText);
            st.muted = true;
            st.mutedWord = st.pickText;
            st.completionPick = 0;
        }
    } else if (byEnter && !enterAccepts) {
        // Enter ohne Auswahl bleibt ein Absatz - das Schreibfeld sieht die
        // Taste nicht, solange die Liste offen ist.
        docops::newParagraph(st.doc, view);
    }
}

}  // namespace

void drawManuscriptWindow(Editor& ed, bool* open) {
    ImGui::SetNextWindowSize(ImVec2(1000, 700), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(TWIN("Manuskript", "manuscript"), open,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                          ImGuiWindowFlags_NoNavInputs)) {
        ImGui::End();
        return;
    }
    if (!ed.project.loaded) {
        ui::textSecondary(TR("Kein Projekt geoeffnet."));
        ImGui::End();
        return;
    }

    ManuscriptState& st = state();
    if (!st.registered) {
        st.doc.views = {&st.main, &st.second};
        st.registered = true;
    }
    st.doc.sync(ed);
    {
        const AppSettings& cfg = theme::settings();
        if (cfg.docSpellCheck || cfg.docAutoCorrect) {
            const std::string tag = resolveLanguage(cfg.docLanguage);
            if (spell::language() != tag) spell::setLanguage(tag);
            // Namen der Elemente gelten als richtig geschrieben
            if (st.namesSignature != st.doc.projectSignature()) {
                std::vector<std::string> names;
                for (const Element& el : ed.project.elements) names.push_back(el.name);
                spell::setKnownNames(names);
                st.namesSignature = st.doc.projectSignature();
            }
        }
    }
    if (!st.split) {
        st.main.activeView = true;
        st.second.activeView = false;
    }
    DocumentView& view = activeView(st);
    const DocOptions opt = makeOptions(st);
    AppSettings& s = theme::settings();
    const long long timeHere = timeAt(st.doc, view.cursor);
    const bool windowFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

    // Tastenkuerzel des Fensters
    if (windowFocused) {
        const ImGuiIO& io = ImGui::GetIO();
        const bool ctrl = io.KeyCtrl && !io.KeyAlt;
        if (ctrl && ImGui::IsKeyPressed(ImGuiKey_F, false)) {
            st.showFind = true;
            st.showReplace = false;
            st.findFocus = true;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_F7, false) && !st.readMode && theme::settings().docSpellCheck) {
            DocumentView& target = activeView(st);
            size_t b = 0, e = 0;
            if (docops::findSpellingError(st.doc, target.selEnd(), &b, &e)) {
                target.select(b, e);
                done(target);
            } else {
                ed.setStatus(TR("Keine Rechtschreibfehler gefunden."));
            }
        }
        if (ctrl && ImGui::IsKeyPressed(ImGuiKey_H, false) && !st.readMode) {
            st.showFind = true;
            st.showReplace = true;
            st.findFocus = true;
        }
    }

    // -------------------------------------------------------------- Band
    if (!st.focusMode) drawRibbon(ed, st, view, opt, timeHere);

    // -------------------------------------------------------------- Suchen
    std::vector<size_t> hits =
        st.showFind ? findAll(st.doc.text(), st.findQuery, st.findCase) : std::vector<size_t>();
    if (st.showFind) {
        if (st.findHit >= static_cast<int>(hits.size())) st.findHit = 0;
        drawFindBar(st, view, hits);
        hits = findAll(st.doc.text(), st.findQuery, st.findCase);  // nach Ersetzen neu
    }
    std::vector<DocHighlight> highlights;
    {
        const ColorScheme& c = theme::colors();
        for (size_t i = 0; i < hits.size(); ++i) {
            const bool current = static_cast<int>(i) == st.findHit;
            highlights.push_back({hits[i], hits[i] + st.findQuery.size(),
                                  theme::u32(theme::withAlpha(c.warningColor, current ? 0.55f : 0.25f))});
        }
    }

    // ------------------------------------------------ Navigation + Seite
    const float statusH = ImGui::GetFrameHeight() + 4.0f + ImGui::GetStyle().ItemSpacing.y;
    const float mainH = std::max(80.0f, ImGui::GetContentRegionAvail().y - statusH);
    if (s.docShowOutline && !st.focusMode) {
        drawNavigation(st, view, mainH);
        ImGui::SameLine();
    }

    completionKeys(st, view);

    ImGui::BeginGroup();
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ImGui::GetStyle().ItemSpacing.x, 0.0f));
    const float rulerH = (s.docShowRuler && !st.focusMode && !st.readMode) ? ImGui::GetFontSize() * 1.5f : 0.0f;
    if (rulerH > 0.0f) activeView(st).drawRuler(st.doc, opt, rulerH);
    const float colW = ImGui::GetContentRegionAvail().x;
    const float viewsH = std::max(40.0f, mainH - rulerH);
    DocEvent ev1, ev2;
    if (st.split) {
        const float splitter = 6.0f;
        const float h1 = std::max(40.0f, (viewsH - splitter) * st.splitRatio);
        ev1 = st.main.draw(st.doc, opt, ImVec2(colW, h1), highlights, st.completionOpen && st.main.activeView);
        ImGui::InvisibleButton("##splitter", ImVec2(colW, splitter));
        if (ImGui::IsItemHovered() || ImGui::IsItemActive()) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
        if (ImGui::IsItemActive())
            st.splitRatio = std::clamp(st.splitRatio + ImGui::GetIO().MouseDelta.y / std::max(1.0f, viewsH), 0.1f, 0.9f);
        const ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
        ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(a.x, a.y + 2.0f), ImVec2(b.x, b.y - 2.0f),
                                                  theme::u32(theme::withAlpha(theme::colors().textSecondary, 0.4f)));
        ev2 = st.second.draw(st.doc, opt, ImVec2(colW, std::max(40.0f, viewsH - h1 - splitter)), highlights,
                             st.completionOpen && st.second.activeView);
        if (st.second.clickedThisFrame) {
            st.second.activeView = true;
            st.main.activeView = false;
        } else if (st.main.clickedThisFrame) {
            st.main.activeView = true;
            st.second.activeView = false;
        }
    } else {
        ev1 = st.main.draw(st.doc, opt, ImVec2(colW, viewsH), highlights, st.completionOpen);
    }
    ImGui::PopStyleVar();
    ImGui::EndGroup();

    DocumentView& active = activeView(st);
    ed.manuscriptKeyboard = active.focused();

    // Klicks auf Marken im Text
    for (DocEvent* ev : {&ev1, &ev2}) {
        switch (ev->kind) {
            case DocEvent::Kind::Element:
                ed.select(SelKind::Element, ev->id);
                ed.focusElementId = ev->id;
                s.showDetails = true;
                break;
            case DocEvent::Kind::Action:
                ed.select(SelKind::Action, ev->id);
                ed.focusActionId = ev->id;
                ed.focusTimeline = true;
                s.showDetails = true;
                break;
            case DocEvent::Kind::Note:
                st.noteDraft = ev->text;
                st.noteBegin = ev->pos;
                st.noteEnd = ev->end;
                st.openNote = true;
                break;
            case DocEvent::Kind::ContextMenu:
                st.openContext = true;
                st.spellWord = ev->spellWord;
                st.spellBegin = ev->spellBegin;
                st.spellEnd = ev->spellEnd;
                st.spellSuggestions = ev->spellWord.empty() ? std::vector<std::string>() : spell::suggest(ev->spellWord);
                break;
            default:
                break;
        }
    }
    for (DocumentView* v : {&st.main, &st.second}) {
        if (v->zoomRequest != 0.0f) {
            s.docZoom = std::clamp(s.docZoom * (1.0f + v->zoomRequest), 0.3f, 4.0f);
            theme::save();
        }
    }

    updateCompletion(ed, st, active, timeHere);

    // ------------------------------------------------------ Statusleiste
    drawStatusBar(ed, st, active, opt, timeAt(st.doc, active.cursor));

    drawDialogs(ed, st, active);
    drawHelpWindow(st);
    ImGui::End();
}

}  // namespace se
