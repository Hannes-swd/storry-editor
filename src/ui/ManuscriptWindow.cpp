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
    bool showTimeMarks = true;   // nur in der Ansicht, nie im fertigen Text
    std::string timeDraft;
    int cursor = 0;              // letzte bekannte Cursorposition
    std::string completionWord;  // gerade getipptes "@..."
    int completionStart = -1;
    int completionPick = 0;
    bool completionOpen = false;
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
        // Wort vor dem Cursor bestimmen, um "@" zu erkennen
        st.completionStart = -1;
        st.completionWord.clear();
        int i = data->CursorPos - 1;
        while (i >= 0) {
            const char c = data->Buf[i];
            if (c == '@') {
                st.completionStart = i;
                st.completionWord = std::string(data->Buf + i + 1, data->Buf + data->CursorPos);
                break;
            }
            if (c == ' ' || c == '\n' || c == '\t') break;
            (void)0;
            --i;
        }
    }
    return 0;
}

void insertAtCursor(ManuscriptState& st, const std::string& text) {
    st.insertRequested = true;
    st.insertText = text;
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
        ImGui::Checkbox(TR("Zeitmarken zeigen"), &st.showTimeMarks);
        ui::tooltip(TR("Nur zur Orientierung beim Lesen - im Export steht sie nie."));
    }
    ImGui::SameLine();
    if (ImGui::Button(TR("Aktion aus Absatz"))) actionFromParagraph(ed, st);
    ui::tooltip(TR("Macht aus dem Absatz am Cursor eine Aktion - Beteiligte und Zeitpunkt kommen "
                   "aus dem Text."));
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
        drawRendered(ed, text, true, st.showTimeMarks);
    } else {
        ImGui::PushStyleColor(ImGuiCol_FrameBg, theme::mix(c.panelBackground, c.backgroundColor, 0.2f));
        const ImGuiInputTextFlags flags =
            ImGuiInputTextFlags_AllowTabInput | ImGuiInputTextFlags_CallbackAlways;
        ImGui::InputTextMultiline("##text", &text, ImVec2(-1, -1), flags, editCallback, &st);
        const bool editing = ImGui::IsItemActive();
        if (ImGui::IsItemEdited()) ed.markManuscript();
        ImGui::PopStyleColor();

        // ------------------------------------------------ Autovervollstaendigung
        if (editing && st.completionStart >= 0) {
            // Nach einem Punkt werden die Felder des Elements angeboten:
            // "@Robert." -> spitzname, alter, ...
            const size_t dot = st.completionWord.find('.');
            std::vector<std::string> matches;   // was eingefuegt wird
            std::vector<std::string> labels;    // was angezeigt wird
            std::vector<ImVec4> colors;
            bool fieldMode = false;

            if (dot != std::string::npos) {
                const std::string elementPart = st.completionWord.substr(0, dot);
                const std::string fieldPart = st.completionWord.substr(dot + 1);
                const Element* owner = nullptr;
                for (const Element& el : ed.project.elements) {
                    if (el.name == elementPart || ed.project.elementPath(el.id) == elementPart)
                        owner = &el;
                }
                if (owner) {
                    fieldMode = true;
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
            if (!fieldMode) {
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
                ImGui::SetNextWindowPos(
                    ImVec2(ImGui::GetItemRectMin().x + 40.0f, ImGui::GetItemRectMin().y + 40.0f),
                    ImGuiCond_Always);
                ImGui::SetNextWindowBgAlpha(0.95f);
                ImGui::Begin("##completion", nullptr,
                             ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize |
                                 ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
                                 ImGuiWindowFlags_NoMove);
                ui::textSecondary(fieldMode ? TR("Feld waehlen - Tab uebernimmt")
                                            : TR("Tab uebernimmt, Esc schliesst"));
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

                if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, true))
                    st.completionPick = (st.completionPick + 1) % static_cast<int>(matches.size());
                if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, true))
                    st.completionPick = (st.completionPick + static_cast<int>(matches.size()) - 1) %
                                        static_cast<int>(matches.size());
                if (ImGui::IsKeyPressed(ImGuiKey_Tab, false)) {
                    const std::string pick =
                        matches[static_cast<size_t>(std::min<int>(st.completionPick,
                                                                  static_cast<int>(matches.size()) - 1))];
                    // das bereits getippte Stueck ersetzen
                    const int from = st.completionStart + 1;
                    if (from <= static_cast<int>(text.size()) && st.cursor >= from)
                        text.erase(static_cast<size_t>(from),
                                   static_cast<size_t>(st.cursor - from));
                    insertAtCursor(st, pick);
                    st.completionPick = 0;
                    ed.markManuscript();
                }
            }
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
