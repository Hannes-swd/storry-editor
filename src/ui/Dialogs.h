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
    std::vector<FieldDef> ownFields;  // nur an diesem Element haengende Felder
    std::string error;
};

struct TemplateDialog {
    bool open = false;
    std::string groupId;
    std::vector<FieldDef> fields;  // working copy, written back on "Alle speichern"
};

// Wohin ein im Feld-Dialog angelegtes Feld gehoert.
// Where a field created in the field dialog belongs.
enum class FieldTarget {
    Template,       // Template der Gruppe
    ElementDialog,  // das gerade offene "Element bearbeiten"
    ElementDirect,  // direkt an ein bestehendes Element (Detail-Panel)
};

struct FieldDialog {
    bool open = false;
    bool isNew = true;
    int index = -1;
    FieldTarget target = FieldTarget::Template;
    std::string elementId;  // nur fuer ElementDirect
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
    std::string startText;
    std::string endText;
    std::string error;
};

// Verwaltung der Verbindungstypen (Liste) und der Editor fuer einen Typ.
struct ConnectionTypeListDialog {
    bool open = false;
};

struct ConnectionTypeDialog {
    bool open = false;
    bool isNew = true;
    ConnectionType draft;
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
    ConnectionTypeListDialog connectionTypes;
    ConnectionTypeDialog connectionType;
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
// Feld-Dialog fuer ein Feld, das nur zu einem Element gehoert.
void openElementField(Editor& ed, FieldTarget target, const std::string& elementId);
void openNewAction(Editor& ed, long long time, const std::string& elementId);
void openEditAction(Editor& ed, const std::string& actionId);
void openNewConnection(Editor& ed, const std::string& src, const std::string& dst);
// Verbindung eines bestimmten Typs setzen, mit vorbelegter Rolle und Zeitpunkt.
void openNewConnectionOfType(Editor& ed, const std::string& typeId, size_t role,
                             const std::string& elementId, bool withTime, long long time);
void openConnectionTypes(Editor& ed);
void openNewConnectionType(Editor& ed);
void openEditConnectionType(Editor& ed, const std::string& typeId);
void openEditConnection(Editor& ed, const std::string& connectionId);
void openNewBlock(Editor& ed, const std::vector<std::string>& connectionIds);
void openMoveElement(Editor& ed, const std::string& elementId);
void confirm(Editor& ed, const std::string& title, const std::string& message,
             const std::string& details, std::function<void()> onConfirm);
void prompt(Editor& ed, const std::string& title, const std::string& label,
            const std::string& value, std::function<void(const std::string&)> onAccept);

}  // namespace dialogs
}  // namespace se
