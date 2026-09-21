#include "ui/UiCommon.h"

#include <algorithm>
#include <cstdio>
#include <map>
#include <cstdlib>

#include "imgui_internal.h"
#include "imgui_stdlib.h"

#include "app/Platform.h"
#include "core/StoryTime.h"
#include "ui/Editor.h"
#include "ui/Lang.h"
#include "ui/Theme.h"

namespace se::ui {

void colorDot(const ImVec4& color, float radius) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    float h = ImGui::GetTextLineHeight();
    ImVec2 center(pos.x + radius + 1.0f, pos.y + h * 0.5f);
    dl->AddCircleFilled(center, radius, theme::u32(color));
    dl->AddCircle(center, radius, theme::u32(theme::colors().nodeOutline), 0, 1.0f);
    ImGui::Dummy(ImVec2(radius * 2.0f + 4.0f, h));
    ImGui::SameLine();
}

void helpMarker(const char* text) {
    ImGui::TextDisabled(TR("(?)"));
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 28.0f);
        ImGui::TextUnformatted(text);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

void textSecondary(const char* text) {
    ImGui::PushStyleColor(ImGuiCol_Text, theme::colors().textSecondary);
    ImGui::TextUnformatted(text);
    ImGui::PopStyleColor();
}

void tooltip(const char* text) {
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) ImGui::SetTooltip("%s", text);
}

std::string ellipsis(const std::string& text, size_t maxChars) {
    if (text.size() <= maxChars) return text;
    return text.substr(0, maxChars) + "...";
}

bool comboStrings(const char* label, const std::vector<std::string>& items, std::string& current,
                  bool allowCustomEmpty) {
    bool changed = false;
    if (ImGui::BeginCombo(label, current.empty() ? TR("<keine>") : current.c_str())) {
        if (allowCustomEmpty) {
            if (ImGui::Selectable(TR("<keine>"), current.empty())) {
                current.clear();
                changed = true;
            }
        }
        for (const std::string& item : items) {
            bool selected = item == current;
            if (ImGui::Selectable(item.c_str(), selected)) {
                current = item;
                changed = true;
            }
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    return changed;
}

bool editableCombo(const char* label, std::vector<std::string>& items, std::string& current,
                   bool allowEmpty, bool* listChanged) {
    // one draft buffer per widget so that typing survives across frames
    static std::map<ImGuiID, std::string> drafts;
    const ImGuiID widgetId = ImGui::GetID(label);
    bool changed = false;

    // the popup must stay tall enough for the "new entry" row below the list
    ImGui::SetNextWindowSizeConstraints(ImVec2(0.0f, 0.0f), ImVec2(9999.0f, 460.0f));
    if (ImGui::BeginCombo(label, current.empty() ? TR("<keine>") : current.c_str(),
                          ImGuiComboFlags_HeightLargest)) {
        if (allowEmpty) {
            if (ImGui::Selectable(TR("<keine>"), current.empty())) {
                current.clear();
                changed = true;
            }
        }
        for (const std::string& item : items) {
            bool selected = item == current;
            if (ImGui::Selectable(item.c_str(), selected)) {
                current = item;
                changed = true;
            }
            if (selected) ImGui::SetItemDefaultFocus();
        }

        ImGui::Separator();
        std::string& draft = drafts[widgetId];
        ImGui::TextUnformatted(TR("Eigener Eintrag:"));
        ImGui::SetNextItemWidth(180.0f);
        bool submitted = ImGui::InputTextWithHint("##newentry", TR("Name eingeben..."), &draft,
                                                  ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine();
        if (ImGui::Button("+")) submitted = true;

        if (submitted) {
            std::string value = trim(draft);
            if (!value.empty()) {
                if (std::find(items.begin(), items.end(), value) == items.end()) {
                    items.push_back(value);
                    if (listChanged) *listChanged = true;
                }
                current = value;
                changed = true;
                draft.clear();
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndCombo();
    }
    return changed;
}

bool tagPicker(Editor& ed, const char* label, std::string& commaSeparated) {
    bool changed = false;
    ImGui::PushID(label);

    std::vector<std::string> selected;
    for (const std::string& part : splitString(commaSeparated, ',')) {
        std::string t = trim(part);
        if (!t.empty()) selected.push_back(t);
    }

    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 130.0f);
    if (ImGui::InputTextWithHint("##tags", TR("Tag1, Tag2, ..."), &commaSeparated)) changed = true;
    ImGui::SameLine();
    if (ImGui::Button(TR("Vorhandene..."))) ImGui::OpenPopup("tag_pick");

    if (ImGui::BeginPopup("tag_pick")) {
        std::vector<std::string> known;
        for (const Action& a : ed.project.actions) {
            for (const std::string& t : a.tags) {
                if (std::find(known.begin(), known.end(), t) == known.end()) known.push_back(t);
            }
        }
        std::sort(known.begin(), known.end());
        if (known.empty()) textSecondary(TR("Noch keine Tags im Projekt - einfach oben eintippen."));
        for (const std::string& t : known) {
            bool on = std::find(selected.begin(), selected.end(), t) != selected.end();
            if (ImGui::Checkbox(t.c_str(), &on)) {
                if (on)
                    selected.push_back(t);
                else
                    selected.erase(std::remove(selected.begin(), selected.end(), t), selected.end());
                commaSeparated = joinList(selected);
                changed = true;
            }
        }
        ImGui::EndPopup();
    }
    ImGui::PopID();
    return changed;
}

// Appends an option to the group (or inherited parent group) that defines the field.
static void addEnumOption(Editor& ed, const std::string& groupId, const std::string& fieldName,
                          const std::string& option) {
    std::string current = groupId;
    int guard = 0;
    while (!current.empty() && guard++ < 64) {
        Group* g = ed.project.group(current);
        if (!g) return;
        for (FieldDef& f : g->fields) {
            if (f.name != fieldName) continue;
            if (std::find(f.enumOptions.begin(), f.enumOptions.end(), option) == f.enumOptions.end()) {
                f.enumOptions.push_back(option);
                ed.markMetadata();
            }
            return;
        }
        current = g->parentId;
    }
}

bool fieldValueEditor(Editor& ed, const FieldDef& field, std::string& value, const char* idSuffix,
                      const std::string& ownerGroupId) {
    ImGui::PushID(idSuffix);
    bool changed = false;
    const float width = ImGui::GetContentRegionAvail().x;

    switch (field.type) {
        case FieldType::Text: {
            bool multiline = value.find('\n') != std::string::npos || value.size() > 60 ||
                             field.name == "description" || field.name == "backstory";
            ImGui::SetNextItemWidth(width);
            if (multiline) {
                changed = ImGui::InputTextMultiline("##text", &value,
                                                    ImVec2(width, ImGui::GetTextLineHeight() * 4.5f));
            } else {
                changed = ImGui::InputText("##text", &value);
            }
            break;
        }
        case FieldType::Integer: {
            int v = std::atoi(value.c_str());
            ImGui::SetNextItemWidth(width * 0.5f);
            if (ImGui::InputInt("##int", &v)) {
                value = std::to_string(v);
                changed = true;
            }
            break;
        }
        case FieldType::Float: {
            float v = static_cast<float>(std::atof(value.c_str()));
            ImGui::SetNextItemWidth(width * 0.5f);
            if (ImGui::InputFloat("##float", &v, 0.1f, 1.0f, "%.3f")) {
                char buf[64];
                std::snprintf(buf, sizeof(buf), "%g", v);
                value = buf;
                changed = true;
            }
            break;
        }
        case FieldType::Date: {
            ImGui::SetNextItemWidth(width * 0.6f);
            changed = ImGui::InputTextWithHint("##date", "Tag 15, 08:00", &value);
            long long minutes = 0;
            ImGui::SameLine();
            if (parseStoryTime(value, &minutes)) {
                ImGui::PushStyleColor(ImGuiCol_Text, theme::colors().successColor);
                ImGui::TextUnformatted(formatStoryTimeLong(minutes).c_str());
                ImGui::PopStyleColor();
            } else if (!value.empty()) {
                ImGui::PushStyleColor(ImGuiCol_Text, theme::colors().warningColor);
                ImGui::TextUnformatted(TR("nicht lesbar"));
                ImGui::PopStyleColor();
            } else {
                textSecondary("leer");
            }
            break;
        }
        case FieldType::Enum: {
            // the option list stays editable right here: a new option is written
            // back into the template of the group that defines the field
            std::vector<std::string> options = field.enumOptions;
            bool optionAdded = false;
            ImGui::SetNextItemWidth(width * 0.6f);
            changed = editableCombo("##enum", options, value, true, &optionAdded);
            if (optionAdded && !ownerGroupId.empty()) {
                for (const std::string& option : options) {
                    if (std::find(field.enumOptions.begin(), field.enumOptions.end(), option) ==
                        field.enumOptions.end())
                        addEnumOption(ed, ownerGroupId, field.name, option);
                }
            }
            break;
        }
        case FieldType::Boolean: {
            bool v = value == "true" || value == "1";
            if (ImGui::Checkbox("##bool", &v)) {
                value = v ? "true" : "false";
                changed = true;
            }
            break;
        }
        case FieldType::List: {
            std::vector<std::string> items = listFromValue(value);
            int removeIndex = -1;
            for (size_t i = 0; i < items.size(); ++i) {
                ImGui::PushID(static_cast<int>(i));
                ImGui::SetNextItemWidth(width - 60.0f);
                if (ImGui::InputText("##item", &items[i])) changed = true;
                ImGui::SameLine();
                if (ImGui::SmallButton("X")) {
                    removeIndex = static_cast<int>(i);
                    changed = true;
                }
                ImGui::PopID();
            }
            if (removeIndex >= 0) items.erase(items.begin() + removeIndex);
            if (ImGui::SmallButton(TR("+ Eintrag"))) {
                items.push_back("");
                changed = true;
            }
            if (changed) value = listToValue(items);
            break;
        }
        case FieldType::Reference: {
            std::string id;
            // stored as "@Group/Path/Name" - resolve back to an element id
            std::string path = value;
            if (!path.empty() && path[0] == '@') path = path.substr(1);
            if (Element* e = ed.project.findElementByPath(path)) id = e->id;
            if (elementCombo(ed, "##ref", id, true)) {
                value = id.empty() ? std::string() : "@" + ed.project.elementPath(id);
                changed = true;
            }
            break;
        }
        case FieldType::File: {
            ImGui::SetNextItemWidth(width - 90.0f);
            changed = ImGui::InputTextWithHint("##file", "Assets/Images/...", &value);
            ImGui::SameLine();
            if (ImGui::Button(TR("Waehlen"))) {
                std::string picked = platform::pickFile(TR("Datei waehlen"), nullptr, nullptr);
                if (!picked.empty()) {
                    value = picked;
                    changed = true;
                }
            }
            break;
        }
    }

    if (!field.description.empty()) {
        ImGui::SameLine();
        helpMarker(field.description.c_str());
    }
    ImGui::PopID();
    return changed;
}

bool elementMultiSelect(Editor& ed, const char* label, std::vector<std::string>& ids,
                        std::string& search, float height) {
    bool changed = false;
    ImGui::PushID(label);
    ImGui::TextUnformatted(label);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    ImGui::InputTextWithHint("##search", TR("Suchen..."), &search);

    ImGui::BeginChild("##list", ImVec2(0, height), ImGuiChildFlags_Borders);
    std::string lastGroup;
    for (const Element& el : ed.project.elements) {
        if (!search.empty() && !iequalsContains(el.name, search)) continue;
        std::string gp = ed.project.groupPath(el.groupId);
        if (gp != lastGroup) {
            lastGroup = gp;
            textSecondary(gp.c_str());
        }
        bool selected = std::find(ids.begin(), ids.end(), el.id) != ids.end();
        colorDot(ed.project.elementColor(el.id));
        if (ImGui::Selectable((el.name + "##" + el.id).c_str(), selected)) {
            if (selected)
                ids.erase(std::remove(ids.begin(), ids.end(), el.id), ids.end());
            else
                ids.push_back(el.id);
            changed = true;
        }
    }
    ImGui::EndChild();

    if (!ids.empty()) {
        std::string names;
        for (size_t i = 0; i < ids.size(); ++i) {
            if (i) names += ", ";
            names += ed.project.displayName(ids[i]);
        }
        textSecondary((TR("Gewaehlt: ") + names).c_str());
    }
    ImGui::PopID();
    return changed;
}

bool elementCombo(Editor& ed, const char* label, std::string& id, bool allowEmpty) {
    bool changed = false;
    std::string preview = id.empty() ? TR("<keins>") : ed.project.elementPath(id);
    if (ImGui::BeginCombo(label, preview.c_str())) {
        if (allowEmpty && ImGui::Selectable(TR("<keins>"), id.empty())) {
            id.clear();
            changed = true;
        }
        for (const Element& el : ed.project.elements) {
            bool selected = el.id == id;
            colorDot(ed.project.elementColor(el.id));
            if (ImGui::Selectable((ed.project.elementPath(el.id) + "##" + el.id).c_str(), selected)) {
                id = el.id;
                changed = true;
            }
        }
        ImGui::EndCombo();
    }
    return changed;
}

bool groupCombo(Editor& ed, const char* label, std::string& groupId, bool allowEmpty,
                const std::string& excludeSubtree) {
    bool changed = false;
    std::string preview = groupId.empty() ? TR("<oberste Ebene>") : ed.project.groupPath(groupId);
    if (ImGui::BeginCombo(label, preview.c_str())) {
        if (allowEmpty && ImGui::Selectable(TR("<oberste Ebene>"), groupId.empty())) {
            groupId.clear();
            changed = true;
        }
        for (const Group& g : ed.project.groups) {
            if (!excludeSubtree.empty() && ed.project.isAncestorGroup(excludeSubtree, g.id)) continue;
            bool selected = g.id == groupId;
            colorDot(ed.project.groupColor(g.id));
            if (ImGui::Selectable((ed.project.groupPath(g.id) + "##" + g.id).c_str(), selected)) {
                groupId = g.id;
                changed = true;
            }
        }
        ImGui::EndCombo();
    }
    return changed;
}

bool timeScrubber(const char* id, long long* time, long long minTime, long long maxTime,
                  const std::vector<std::pair<long long, ImVec4>>& marks, float height) {
    const ColorScheme& c = theme::colors();
    if (maxTime <= minTime) maxTime = minTime + kMinutesPerDay;

    const float width = std::max(120.0f, ImGui::GetContentRegionAvail().x);
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton(id, ImVec2(width, height));
    const bool active = ImGui::IsItemActive();
    const bool hovered = ImGui::IsItemHovered();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    const float span = static_cast<float>(maxTime - minTime);
    auto xOf = [&](long long t) {
        return pos.x + (static_cast<float>(t - minTime) / span) * width;
    };

    bool changed = false;
    if (active && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        const float f = std::min(1.0f, std::max(0.0f, (ImGui::GetMousePos().x - pos.x) / width));
        const long long picked = minTime + static_cast<long long>(f * span);
        if (picked != *time) {
            *time = picked;
            changed = true;
        }
    }
    if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);

    // Spur
    const float trackTop = pos.y + height * 0.55f;
    const float trackBottom = trackTop + 6.0f;
    dl->AddRectFilled(ImVec2(pos.x, trackTop), ImVec2(pos.x + width, trackBottom),
                      theme::u32(c.timelineTrackAlt), 3.0f);
    dl->AddRect(ImVec2(pos.x, trackTop), ImVec2(pos.x + width, trackBottom),
                theme::u32(c.timelineGrid), 3.0f);

    // Tagesstriche, solange sie nicht zu dicht stehen
    const long long days = (maxTime - minTime) / kMinutesPerDay + 1;
    if (days > 0 && width / static_cast<float>(days) > 6.0f) {
        for (long long d = 0; d <= days; ++d) {
            const float x = xOf(minTime + d * kMinutesPerDay);
            dl->AddLine(ImVec2(x, trackTop - 2.0f), ImVec2(x, trackBottom + 2.0f),
                        theme::u32(c.timelineGrid, 0.7f));
        }
    }

    // Ereignisse auf der Spur
    for (const auto& mark : marks) {
        const float x = xOf(mark.first);
        dl->AddLine(ImVec2(x, trackTop - 5.0f), ImVec2(x, trackBottom + 5.0f),
                    theme::u32(mark.second), 2.0f);
    }

    // Griff
    const float handleX = xOf(*time);
    dl->AddLine(ImVec2(handleX, pos.y + 2.0f), ImVec2(handleX, pos.y + height - 2.0f),
                theme::u32(c.accentColor), 2.5f);
    dl->AddTriangleFilled(ImVec2(handleX - 5.0f, pos.y + 2.0f),
                          ImVec2(handleX + 5.0f, pos.y + 2.0f),
                          ImVec2(handleX, pos.y + 10.0f), theme::u32(c.accentColor));

    const std::string label = formatStoryTime(*time);
    const ImVec2 ts = ImGui::CalcTextSize(label.c_str());
    float labelX = std::min(pos.x + width - ts.x - 2.0f, std::max(pos.x + 2.0f, handleX - ts.x * 0.5f));
    dl->AddRectFilled(ImVec2(labelX - 3.0f, pos.y + 1.0f),
                      ImVec2(labelX + ts.x + 3.0f, pos.y + ts.y + 1.0f),
                      theme::u32(theme::withAlpha(c.backgroundColor, 0.85f)), 3.0f);
    dl->AddText(ImVec2(labelX, pos.y + 1.0f), theme::u32(c.textPrimary), label.c_str());

    return changed;
}

void elementChip(Editor& ed, const std::string& elementId, bool sameLine) {
    const Element* el = ed.project.element(elementId);
    if (!el) return;
    if (sameLine && ImGui::GetCursorPosX() > ImGui::GetStyle().WindowPadding.x) ImGui::SameLine();
    ImVec4 col = ed.project.elementColor(elementId);
    ImGui::PushStyleColor(ImGuiCol_Button, theme::withAlpha(col, 0.35f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, theme::withAlpha(col, 0.55f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, col);
    if (ImGui::SmallButton((el->name + "##chip" + elementId).c_str())) {
        ed.select(SelKind::Element, elementId);
        ed.focusElementId = elementId;
    }
    ImGui::PopStyleColor(3);
    tooltip(ed.project.elementPath(elementId).c_str());
}

}  // namespace se::ui
