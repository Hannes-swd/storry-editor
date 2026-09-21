// Central editor state: the project, selection, undo stack and the save queue.
#pragma once

#include <deque>
#include <set>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "core/FileWatcher.h"
#include "core/Project.h"
#include "ui/Dialogs.h"

namespace se {

enum class SelKind { None, Group, Element, Action, Connection, Block };

struct Selection {
    SelKind kind = SelKind::None;
    std::string id;
    bool is(SelKind k, const std::string& other) const { return kind == k && id == other; }
};

class Editor {
public:
    Project project;
    FileWatcher watcher;
    Selection selection;
    DialogState dialogs;

    // navigation requests between windows
    std::string focusElementId;   // group manager scrolls to it
    std::string focusActionId;    // actions panel + timeline scroll to it
    bool focusTimeline = false;

    std::vector<std::string> droppedFiles;  // filled by WM_DROPFILES
    bool quitRequested = false;
    bool resetLayoutRequested = false;
    std::vector<ExternalChange> pendingConflicts;

    void init();
    void shutdown();
    void newFrame(float dt);
    void drawMainMenuBar();
    void drawWindows();
    void handleShortcuts();

    // ------------------------------------------------------- project lifecycle
    bool createProject(const std::string& folder, const std::string& name);
    bool openProject(const std::string& folder);
    void closeProject();
    void saveEverything();

    // -------------------------------------------------------------- commands
    void pushUndo(const std::string& label);
    void undo();
    void redo();
    bool canUndo() const { return !undoStack_.empty(); }
    bool canRedo() const { return !redoStack_.empty(); }
    std::string undoLabel() const { return undoStack_.empty() ? "" : undoStack_.back().label; }
    std::string redoLabel() const { return redoStack_.empty() ? "" : redoStack_.back().label; }

    void markElement(const std::string& id);
    void markAllElements();
    void markActions();
    void markConnections();
    void markMetadata();
    void markManuscript();
    void flushSaves();

    void select(SelKind kind, const std::string& id);
    void setStatus(const std::string& text, bool error = false);
    const std::string& status() const { return status_; }
    bool statusIsError() const { return statusError_; }
    float statusAge() const { return statusTimer_; }

    void deleteElementWithFile(const std::string& id);
    void renameElement(const std::string& id, const std::string& newName);
    void moveElement(const std::string& id, const std::string& newGroupId);

private:
    struct UndoEntry {
        std::string label;
        nlohmann::json state;
    };

    std::deque<UndoEntry> undoStack_;
    std::deque<UndoEntry> redoStack_;
    std::set<std::string> dirtyElements_;
    bool dirtyActions_ = false;
    bool dirtyConnections_ = false;
    bool dirtyMetadata_ = false;
    bool dirtyManuscript_ = false;
    bool dirtyAll_ = false;
    std::string status_;
    bool statusError_ = false;
    float statusTimer_ = 0.0f;
};

}  // namespace se
