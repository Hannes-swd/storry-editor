// Story visualizer (spec 3.3): read only, narrative view of the same actions.
#include <algorithm>
#include <string>
#include <vector>

#include "imgui.h"
#include "imgui_stdlib.h"

#include "core/StoryTime.h"
#include "ui/Editor.h"
#include "ui/Lang.h"
#include "ui/Theme.h"
#include "ui/UiCommon.h"
#include "ui/Windows.h"

namespace se {
namespace {

struct StoryState {
    std::string focusElementId;
    std::string storyline;
    bool onlyMajor = false;
    bool fullText = false;
    bool showMutations = true;
};

StoryState& state() {
    static StoryState s;
    return s;
}

std::string firstSentences(const std::string& text, int count) {
    int found = 0;
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '.' || text[i] == '!' || text[i] == '?') {
            ++found;
            if (found >= count) return text.substr(0, i + 1);
        }
    }
    return text;
}

}  // namespace

void drawStoryVisualizerWindow(Editor& ed, bool* open) {
    ImGui::SetNextWindowSize(ImVec2(620, 560), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(TWIN("Story Visualizer", "story"), open)) {
        ImGui::End();
        return;
    }
    if (!ed.project.loaded) {
        ui::textSecondary(TR("Kein Projekt geoeffnet."));
        ImGui::End();
        return;
    }

    StoryState& st = state();
    ColorScheme& c = theme::colors();

    ImGui::SetNextItemWidth(220.0f);
    ui::elementCombo(ed, TR("Fokus"), st.focusElementId, true);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(160.0f);
    ui::comboStrings(TR("Strang"), ed.project.storylines, st.storyline, true);
    ImGui::SameLine();
    ImGui::Checkbox(TR("nur Wichtiges"), &st.onlyMajor);
    ImGui::SameLine();
    ImGui::Checkbox(TR("voller Text"), &st.fullText);
    ImGui::SameLine();
    ImGui::Checkbox(TR("Werteaenderungen"), &st.showMutations);
    ImGui::Separator();
    ui::textSecondary(TR("Nur-Lese-Ansicht: hier wird die Geschichte erzaehlt, nicht bearbeitet."));

    ImGui::BeginChild("story", ImVec2(0, 0), ImGuiChildFlags_Borders);

    std::vector<const Action*> all = ed.project.sortedActions();
    std::vector<const Action*> shown;
    long long skippedFrom = -1;
    long long lastShownTime = -1;
    long long lastDay = -1;

    auto matches = [&](const Action* a) {
        if (st.onlyMajor && !a->major) return false;
        if (!st.storyline.empty() && a->storyline != st.storyline) return false;
        if (!st.focusElementId.empty()) {
            bool hit = std::find(a->elementIds.begin(), a->elementIds.end(), st.focusElementId) !=
                       a->elementIds.end();
            for (const Mutation& m : a->mutations) {
                if (m.elementId == st.focusElementId) hit = true;
            }
            if (!hit) return false;
        }
        return true;
    };

    for (const Action* a : all) {
        long long t = ed.project.resolveActionTime(*a);
        if (!matches(a)) {
            if (skippedFrom < 0) skippedFrom = t;
            continue;
        }

        // gap summary between two visible events
        if (lastShownTime >= 0) {
            long long gap = t - lastShownTime;
            if (gap >= kMinutesPerDay) {
                ImGui::Spacing();
                ImGui::PushStyleColor(ImGuiCol_Text, c.textSecondary);
                std::string label = "--- " + formatDuration(gap) + TR(" vergehen");
                if (skippedFrom >= 0) label += TR(" (nicht gezeigte Ereignisse dazwischen)");
                label += " ---";
                ImGui::TextUnformatted(label.c_str());
                ImGui::PopStyleColor();
                ImGui::Spacing();
            }
        }
        skippedFrom = -1;

        if (dayOf(t) != lastDay) {
            lastDay = dayOf(t);
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text, c.accentColor);
            ImGui::PushFont(nullptr, ImGui::GetFontSize() * 1.15f);
            ImGui::TextUnformatted(("--- " + formatDayHeadline(t) + " ---").c_str());
            ImGui::PopFont();
            ImGui::PopStyleColor();
        }

        ImGui::PushID(a->id.c_str());
        ImVec4 col = a->elementIds.empty() ? c.textPrimary : ed.project.colorForId(a->elementIds.front());
        ImGui::PushStyleColor(ImGuiCol_Text, col);
        ImGui::TextUnformatted("->");
        ImGui::PopStyleColor();
        ImGui::SameLine();
        if (ImGui::Selectable(a->title.c_str(), ed.selection.is(SelKind::Action, a->id))) {
            ed.select(SelKind::Action, a->id);
            theme::settings().showDetails = true;
        }
        ImGui::SameLine();
        ui::textSecondary(formatStoryTime(t).c_str());

        if (!a->description.empty()) {
            ImGui::Indent(18.0f);
            ImGui::PushStyleColor(ImGuiCol_Text, c.textSecondary);
            ImGui::TextWrapped("%s", st.fullText ? a->description.c_str()
                                                 : firstSentences(a->description, 3).c_str());
            ImGui::PopStyleColor();
            ImGui::Unindent(18.0f);
        }

        if (st.showMutations && !a->mutations.empty()) {
            ImGui::Indent(18.0f);
            for (const Mutation& m : a->mutations) {
                ImGui::PushStyleColor(ImGuiCol_Text, c.successColor);
                ImGui::Text("* %s.%s: %s -> %s", ed.project.displayName(m.elementId).c_str(),
                            m.field.c_str(), m.oldValue.c_str(), m.newValue.c_str());
                ImGui::PopStyleColor();
            }
            ImGui::Unindent(18.0f);
        }

        if (!a->elementIds.empty()) {
            ImGui::Indent(18.0f);
            for (const std::string& id : a->elementIds) ui::elementChip(ed, id);
            ImGui::NewLine();
            ImGui::Unindent(18.0f);
        }
        ImGui::PopID();

        shown.push_back(a);
        lastShownTime = t;
    }

    if (shown.empty()) ui::textSecondary(TR("Keine Ereignisse fuer diese Auswahl."));
    ImGui::EndChild();
    ImGui::End();
}

}  // namespace se
