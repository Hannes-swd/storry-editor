// Actions panel (spec 3.5): the table view on the same data the timeline shows.
#include <algorithm>
#include <set>
#include <string>
#include <vector>

#include "imgui.h"
#include "imgui_stdlib.h"

#include "app/Platform.h"
#include "core/StoryTime.h"
#include "core/VaultIO.h"
#include "ui/Dialogs.h"
#include "ui/Editor.h"
#include "ui/Lang.h"
#include "ui/Theme.h"
#include "ui/UiCommon.h"
#include "ui/Windows.h"

namespace se {
namespace {

struct ActionsState {
    std::string search;
    std::vector<std::string> elementFilter;
    std::string elementSearch;
    std::string tagFilter;
    std::string typeFilter;
    bool useTimeFilter = false;
    std::string fromText = "Tag 1, 00:00";
    std::string toText = "Tag 30, 00:00";
    std::set<std::string> expanded;
    int sortColumn = 2;
    bool sortAscending = true;
};

ActionsState& state() {
    static ActionsState s;
    return s;
}

std::vector<std::string> allTags(const Project& p) {
    std::set<std::string> tags;
    for (const Action& a : p.actions) {
        for (const std::string& t : a.tags) tags.insert(t);
    }
    return std::vector<std::string>(tags.begin(), tags.end());
}

bool passesFilters(Editor& ed, const Action& a, ActionsState& st, long long fromT, long long toT) {
    if (!st.search.empty() && !iequalsContains(a.title, st.search) &&
        !iequalsContains(a.description, st.search))
        return false;
    if (!st.typeFilter.empty() && a.type != st.typeFilter) return false;
    if (!st.tagFilter.empty() &&
        std::find(a.tags.begin(), a.tags.end(), st.tagFilter) == a.tags.end())
        return false;
    for (const std::string& needed : st.elementFilter) {
        bool found = std::find(a.elementIds.begin(), a.elementIds.end(), needed) != a.elementIds.end();
        for (const Mutation& m : a.mutations) {
            if (m.elementId == needed) found = true;
        }
        if (!found) return false;
    }
    if (st.useTimeFilter) {
        long long t = ed.project.resolveActionTime(a);
        if (t < fromT || t > toT) return false;
    }
    return true;
}

void drawExpandedRow(Editor& ed, const Action& a) {
    ColorScheme& c = theme::colors();
    ImGui::Indent(12.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, c.textSecondary);
    ImGui::TextUnformatted((TR("Zeit: ") + formatStoryTimeLong(ed.project.resolveActionTime(a))).c_str());
    ImGui::PopStyleColor();
    if (!a.description.empty()) ImGui::TextWrapped("%s", a.description.c_str());

    if (!a.elementIds.empty()) {
        ImGui::TextUnformatted(TR("Beteiligte:"));
        for (const std::string& id : a.elementIds) ui::elementChip(ed, id);
        ImGui::NewLine();
    }
    if (!a.tags.empty()) ui::textSecondary((TR("Tags: ") + joinList(a.tags)).c_str());
    if (!a.attachments.empty()) {
        ui::textSecondary((TR("Anhaenge: ") + std::to_string(a.attachments.size())).c_str());
        for (const std::string& att : a.attachments) {
            ImGui::PushID(att.c_str());
            if (ImGui::SmallButton(att.c_str()))
                platform::openInShell(vault::absolutePath(ed.project, att));
            ImGui::PopID();
        }
    }
    if (!a.mutations.empty()) {
        ImGui::TextUnformatted(TR("Attribut-Aenderungen:"));
        for (const Mutation& m : a.mutations) {
            ImGui::BulletText("%s.%s: %s -> %s", ed.project.displayName(m.elementId).c_str(),
                              m.field.c_str(), m.oldValue.c_str(), m.newValue.c_str());
        }
    }

    ImGui::PushID(a.id.c_str());
    if (ImGui::SmallButton(TR("Bearbeiten"))) dialogs::openEditAction(ed, a.id);
    ImGui::SameLine();
    if (ImGui::SmallButton(TR("Duplizieren"))) {
        ed.pushUndo(TR("Aktion dupliziert"));
        Action copy = a;
        copy.id = newId("act");
        copy.title += TR(" (Kopie)");
        ed.project.actions.push_back(copy);
        ed.markActions();
    }
    ImGui::SameLine();
    if (ImGui::SmallButton(TR("Zur Timeline"))) {
        ed.focusActionId = a.id;
        ed.focusTimeline = true;
        ed.select(SelKind::Action, a.id);
        theme::settings().showTimeline = true;
    }
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, theme::withAlpha(c.errorColor, 0.7f));
    if (ImGui::SmallButton(TR("Loeschen"))) {
        std::string id = a.id;
        std::string title = a.title;
        dialogs::confirm(ed, TR("Aktion loeschen"), TR("Aktion \"") + title + TR("\" loeschen?"), "",
                         [&ed, id]() {
                             ed.pushUndo(TR("Aktion geloescht"));
                             ed.project.removeAction(id);
                             ed.markActions();
                         });
    }
    ImGui::PopStyleColor();
    ImGui::PopID();
    ImGui::Unindent(12.0f);
    ImGui::Spacing();
}

}  // namespace

void drawActionsWindow(Editor& ed, bool* open) {
    ImGui::SetNextWindowSize(ImVec2(820, 420), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(TWIN("Aktionen", "actions"), open)) {
        ImGui::End();
        return;
    }
    if (!ed.project.loaded) {
        ui::textSecondary(TR("Kein Projekt geoeffnet."));
        ImGui::End();
        return;
    }

    ActionsState& st = state();

    // ------------------------------------------------------------ filter bar
    if (ImGui::Button(TR("+ Neue Aktion"))) dialogs::openNewAction(ed, 0, "");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(180.0f);
    ImGui::InputTextWithHint("##search", TR("Volltextsuche..."), &st.search);
    ImGui::SameLine();
    bool elementFilterActive = !st.elementFilter.empty();
    if (elementFilterActive) ImGui::PushStyleColor(ImGuiCol_Button, theme::accentFill());
    if (ImGui::Button(TR("Beteiligte filtern"))) ImGui::OpenPopup("act_elements");
    if (elementFilterActive) ImGui::PopStyleColor();
    if (ImGui::BeginPopup("act_elements")) {
        ui::elementMultiSelect(ed, TR("Elemente"), st.elementFilter, st.elementSearch, 220.0f);
        if (ImGui::Button(TR("Leeren"))) st.elementFilter.clear();
        ImGui::EndPopup();
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(130.0f);
    ui::comboStrings("##type", ed.project.actionTypes, st.typeFilter, true);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(130.0f);
    std::vector<std::string> tags = allTags(ed.project);
    ui::comboStrings("##tag", tags, st.tagFilter, true);
    ImGui::SameLine();
    ImGui::Checkbox(TR("Zeitraum"), &st.useTimeFilter);
    long long fromT = 0, toT = 0;
    if (st.useTimeFilter) {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(110.0f);
        ImGui::InputText("##from", &st.fromText);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(110.0f);
        ImGui::InputText("##to", &st.toText);
        if (!parseStoryTime(st.fromText, &fromT)) fromT = 0;
        if (!parseStoryTime(st.toText, &toT)) toT = fromT + 30 * kMinutesPerDay;
    }
    ImGui::SameLine();
    if (ImGui::Button(TR("Filter zuruecksetzen"))) {
        st = ActionsState{};
    }

    // ----------------------------------------------------------- collection
    std::vector<const Action*> rows;
    for (const Action& a : ed.project.actions) {
        if (passesFilters(ed, a, st, fromT, toT)) rows.push_back(&a);
    }

    auto participants = [&ed](const Action* a) {
        std::string s;
        for (size_t i = 0; i < a->elementIds.size(); ++i) {
            if (i) s += ", ";
            s += ed.project.displayName(a->elementIds[i]);
        }
        return s;
    };

    std::sort(rows.begin(), rows.end(), [&](const Action* l, const Action* r) {
        bool less = false;
        switch (st.sortColumn) {
            case 1: less = l->title < r->title; break;
            case 2: less = ed.project.resolveActionTime(*l) < ed.project.resolveActionTime(*r); break;
            case 3: less = l->type < r->type; break;
            case 4: less = participants(l) < participants(r); break;
            case 5: less = l->attachments.size() < r->attachments.size(); break;
            default: less = l->id < r->id; break;
        }
        return st.sortAscending ? less : !less;
    });

    ImGui::Text(TR("%d von %d Aktionen"), static_cast<int>(rows.size()),
                static_cast<int>(ed.project.actions.size()));

    ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable |
                            ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp;
    if (ImGui::BeginTable("actions", 6, flags)) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 40.0f);
        ImGui::TableSetupColumn(TR("Titel"), ImGuiTableColumnFlags_WidthStretch, 3.0f);
        ImGui::TableSetupColumn(TR("Zeit"), ImGuiTableColumnFlags_WidthFixed, 130.0f);
        ImGui::TableSetupColumn(TR("Typ"), ImGuiTableColumnFlags_WidthFixed, 110.0f);
        ImGui::TableSetupColumn(TR("Beteiligte"), ImGuiTableColumnFlags_WidthStretch, 2.5f);
        ImGui::TableSetupColumn(TR("Details"), ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoSort,
                                70.0f);
        ImGui::TableHeadersRow();

        if (ImGuiTableSortSpecs* specs = ImGui::TableGetSortSpecs()) {
            if (specs->SpecsDirty && specs->SpecsCount > 0) {
                st.sortColumn = specs->Specs[0].ColumnIndex;
                st.sortAscending = specs->Specs[0].SortDirection == ImGuiSortDirection_Ascending;
                specs->SpecsDirty = false;
            }
        }

        int index = 0;
        for (const Action* a : rows) {
            ++index;
            ImGui::PushID(a->id.c_str());
            ImGui::TableNextRow();

            bool selected = ed.selection.is(SelKind::Action, a->id);
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("#%d", index);

            ImGui::TableSetColumnIndex(1);
            if (ImGui::Selectable(a->title.c_str(), selected,
                                  ImGuiSelectableFlags_SpanAllColumns |
                                      ImGuiSelectableFlags_AllowDoubleClick)) {
                ed.select(SelKind::Action, a->id);
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                    dialogs::openEditAction(ed, a->id);
            }
            if (ed.focusActionId == a->id) {
                ImGui::SetScrollHereY(0.4f);
                ed.focusActionId.clear();
            }

            ImGui::TableSetColumnIndex(2);
            ImGui::TextUnformatted(formatStoryTime(ed.project.resolveActionTime(*a)).c_str());

            ImGui::TableSetColumnIndex(3);
            ImGui::TextUnformatted(a->type.c_str());

            ImGui::TableSetColumnIndex(4);
            ImGui::TextUnformatted(ui::ellipsis(participants(a), 48).c_str());

            ImGui::TableSetColumnIndex(5);
            bool isExpanded = st.expanded.count(a->id) > 0;
            if (ImGui::SmallButton(isExpanded ? TR("zu") : TR("auf"))) {
                if (isExpanded)
                    st.expanded.erase(a->id);
                else
                    st.expanded.insert(a->id);
            }

            if (st.expanded.count(a->id) > 0) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(1);
                drawExpandedRow(ed, *a);
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    ImGui::End();
}

}  // namespace se
