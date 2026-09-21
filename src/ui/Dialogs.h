// All modal/popup dialogs of the editor.
#pragma once

#include <functional>
#include <map>
#include <string>
#include <vector>

#include "core/FileWatcher.h"
#include "core/Model.h"

namespace se {

class Editor;

struct NewProjectDialog {
    bool open = false;
    std::string name = "Meine Geschichte";
    std::string folder;
};

struct GroupDialog {
    bool open = false;
    bool isNew = true;
    std::string groupId;
    std::string parentId;
    std::string name;
    ImVec4 color = ImVec4(0.5f, 0.6f, 0.8f, 1.0f);
    bool explicitColor = false;
};

struct ElementDialog {
    bool open = false;
    bool isNew = true;
    std::string groupId;
    std::string elementId;
    std::string name;
    std::map<std::string, std::string> values;
    std::string error;
};

struct TemplateDialog {
    bool open = false;
    std::string groupId;
    std::vector<FieldDef> fields;  // working copy, written back on "Alle speichern"
};

struct FieldDialog {
    bool open = false;
    bool isNew = true;
    int index = -1;
    FieldDef field;
    std::string enumOptions;  // one option per line
    std::string error;
};

struct ActionDialog {
    bool open = false;
    bool isNew = true;
    Action draft;
    std::string timeText;
    std::string tagsText;
    std::string elementSearch;
    std::string error;
};

struct ConnectionDialog {
    bool open = false;
    bool isNew = true;
    Connection draft;
};

struct BlockDialog {
    bool open = false;
    bool isNew = true;
    Block draft;
    std::vector<std::string> connectionIds;
};

struct ConfirmDialog {
    bool open = false;
    std::string title;
    std::string message;
    std::string details;
    std::function<void()> onConfirm;
};

struct ConflictDialog {
    bool open = false;
    ExternalChange change;
    std::string externalText;
};

struct MoveDialog {
    bool open = false;
    std::string elementId;
    std::string targetGroupId;
};

struct TextPromptDialog {
    bool open = false;
    std::string title;
    std::string label;
    std::string value;
    std::function<void(const std::string&)> onAccept;
};

struct DialogState {
    NewProjectDialog newProject;
    GroupDialog group;
    ElementDialog element;
    TemplateDialog templateEditor;
    FieldDialog field;
    ActionDialog action;
    ConnectionDialog connection;
    BlockDialog block;
    ConfirmDialog confirm;
    ConflictDialog conflict;
    MoveDialog move;
    TextPromptDialog prompt;
    bool about = false;
    bool shortcuts = false;
};

namespace dialogs {

void draw(Editor& ed);

// request helpers -----------------------------------------------------------
void openNewProject(Editor& ed);
void openNewGroup(Editor& ed, const std::string& parentId);
void openEditGroup(Editor& ed, const std::string& groupId);
void openNewElement(Editor& ed, const std::string& groupId);
void openEditElement(Editor& ed, const std::string& elementId);
void openTemplate(Editor& ed, const std::string& groupId);
void openNewAction(Editor& ed, long long time, const std::string& elementId);
void openEditAction(Editor& ed, const std::string& actionId);
void openNewConnection(Editor& ed, const std::string& src, const std::string& dst);
void openEditConnection(Editor& ed, const std::string& connectionId);
void openNewBlock(Editor& ed, const std::vector<std::string>& connectionIds);
void openMoveElement(Editor& ed, const std::string& elementId);
void confirm(Editor& ed, const std::string& title, const std::string& message,
             const std::string& details, std::function<void()> onConfirm);
void prompt(Editor& ed, const std::string& title, const std::string& label,
            const std::string& value, std::function<void(const std::string&)> onAccept);

}  // namespace dialogs
}  // namespace se
