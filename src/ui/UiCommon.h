// Small shared widgets used by more than one window.
#pragma once

#include <string>
#include <vector>

#include "imgui.h"

#include "core/Model.h"

namespace se {

class Editor;

namespace ui {

void colorDot(const ImVec4& color, float radius = 5.0f);
// Senkrechter Strich zwischen zwei Gruppen einer Werkzeugleiste.
void verticalSeparator();
void helpMarker(const char* text);
void textSecondary(const char* text);
void tooltip(const char* text);
std::string ellipsis(const std::string& text, size_t maxChars);

// Read-only selection over an existing list (used for filters).
bool comboStrings(const char* label, const std::vector<std::string>& items, std::string& current,
                  bool allowCustomEmpty = false);

// Selection over a list the user may extend: the popup carries an input field
// for a new entry. `listChanged` reports that `items` grew, so the caller can
// persist the list.
bool editableCombo(const char* label, std::vector<std::string>& items, std::string& current,
                   bool allowEmpty, bool* listChanged = nullptr);

// Comma separated tag input plus a picker over the tags already used anywhere.
bool tagPicker(Editor& ed, const char* label, std::string& commaSeparated);

// Editor for one template field value. Returns true when the value changed.
// `ownerGroupId` lets Enum fields gain new options straight from the dropdown.
bool fieldValueEditor(Editor& ed, const FieldDef& field, std::string& value, const char* idSuffix,
                      const std::string& ownerGroupId = std::string());

// Multi select over all elements of the project (grouped by group path).
bool elementMultiSelect(Editor& ed, const char* label, std::vector<std::string>& ids,
                        std::string& search, float height = 150.0f);
bool elementCombo(Editor& ed, const char* label, std::string& id, bool allowEmpty = true);
bool groupCombo(Editor& ed, const char* label, std::string& groupId, bool allowEmpty = true,
                const std::string& excludeSubtree = std::string());

// Frei beweglicher Zeitstrahl: ziehen setzt den Zeitpunkt minutengenau.
// `marks` sind Zeitpunkte, die als farbige Striche auf der Spur erscheinen.
// A freely draggable time track: dragging sets the time to the minute.
bool timeScrubber(const char* id, long long* time, long long minTime, long long maxTime,
                  const std::vector<std::pair<long long, ImVec4>>& marks, float height = 30.0f);

// Coloured, clickable reference to an element; selects it when clicked.
void elementChip(Editor& ed, const std::string& elementId, bool sameLine = true);

}  // namespace ui
}  // namespace se
