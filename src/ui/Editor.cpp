#include "ui/Editor.h"

#include <algorithm>
#include <map>

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_stdlib.h"

#include "app/Platform.h"
#include "core/StoryTime.h"
#include "core/VaultIO.h"
#include "ui/Theme.h"
#include "ui/UiCommon.h"
#include "ui/Windows.h"

namespace se {
namespace {
constexpr size_t kMaxUndo = 64;
}

void Editor::init() {
    AppSettings& s = theme::settings();
    if (!s.lastVault.empty()) {
        std::string err;
        if (vault::load(s.lastVault, project, &err)) {
            watcher.reset(project);
            setStatus("Projekt geladen: " + project.name);
        } else {
            s.lastVault.clear();
        }
    }
}

void Editor::shutdown() {
    if (project.loaded) flushSaves();
    theme::settings().lastVault = project.vaultPath;
    theme::settings().lastProjectName = project.name;
    theme::save();
}

void Editor::newFrame(float dt) {
    if (statusTimer_ > 0.0f) statusTimer_ -= dt;

    if (project.loaded) {
        std::vector<ExternalChange> changes = watcher.poll(project, dt);
        for (const ExternalChange& c : changes) {
            bool known = false;
            for (const auto& p : pendingConflicts) {
                if (p.elementId == c.elementId) known = true;
            }
            if (!known) pendingConflicts.push_back(c);
        }
        if (!pendingConflicts.empty() && !dialogs.conflict.open) {
            dialogs.conflict.open = true;
            dialogs.conflict.change = pendingConflicts.front();
            dialogs.conflict.externalText.clear();
            platform::readFile(vault::absolutePath(project, dialogs.conflict.change.relativePath),
                               &dialogs.conflict.externalText);
            pendingConflicts.erase(pendingConflicts.begin());
        }
    }
}

// ----------------------------------------------------------------- commands
void Editor::pushUndo(const std::string& label) {
    if (!project.loaded) return;
    undoStack_.push_back({label, vault::snapshot(project)});
    if (undoStack_.size() > kMaxUndo) undoStack_.pop_front();
    redoStack_.clear();
}

void Editor::undo() {
    if (undoStack_.empty()) return;
    UndoEntry entry = undoStack_.back();
    undoStack_.pop_back();
    redoStack_.push_back({entry.label, vault::snapshot(project)});
    vault::restore(entry.state, project);
    dirtyAll_ = true;
    flushSaves();
    watcher.reset(project);
    setStatus("Rueckgaengig: " + entry.label);
}

void Editor::redo() {
    if (redoStack_.empty()) return;
    UndoEntry entry = redoStack_.back();
    redoStack_.pop_back();
    undoStack_.push_back({entry.label, vault::snapshot(project)});
    vault::restore(entry.state, project);
    dirtyAll_ = true;
    flushSaves();
    watcher.reset(project);
    setStatus("Wiederholt: " + entry.label);
}

void Editor::markElement(const std::string& id) {
    dirtyElements_.insert(id);
    dirtyActions_ = true;  // linked-event lists inside other files may change
}
void Editor::markAllElements() { dirtyAll_ = true; }
void Editor::markActions() {
    dirtyActions_ = true;
    dirtyAll_ = true;  // element files list their linked actions
}
void Editor::markConnections() {
    dirtyConnections_ = true;
    dirtyAll_ = true;  // element files list their relations
}
void Editor::markMetadata() { dirtyMetadata_ = true; }

void Editor::flushSaves() {
    if (!project.loaded || project.vaultPath.empty()) return;
    if (!theme::settings().autosave && !dirtyAll_ && dirtyElements_.empty() && !dirtyActions_ &&
        !dirtyConnections_ && !dirtyMetadata_)
        return;

    std::string err;
    bool ok = true;
    if (dirtyAll_) {
        ok = vault::saveAll(project, &err);
        for (const Element& el : project.elements) watcher.touch(project, el);
    } else {
        if (dirtyMetadata_) ok = vault::saveMetadata(project, &err) && ok;
        if (dirtyActions_) ok = vault::saveActions(project, &err) && ok;
        if (dirtyConnections_) ok = vault::saveConnections(project, &err) && ok;
        for (const std::string& id : dirtyElements_) {
            Element* el = project.element(id);
            if (!el) continue;
            ok = vault::saveElement(project, *el, &err) && ok;
            watcher.touch(project, *el);
        }
    }
    dirtyAll_ = false;
    dirtyMetadata_ = false;
    dirtyActions_ = false;
    dirtyConnections_ = false;
    dirtyElements_.clear();
    if (!ok && !err.empty()) setStatus(err, true);
}

void Editor::select(SelKind kind, const std::string& id) {
    selection.kind = kind;
    selection.id = id;
}

void Editor::setStatus(const std::string& text, bool error) {
    status_ = text;
    statusError_ = error;
    statusTimer_ = 6.0f;
}

// -------------------------------------------------------- project lifecycle
bool Editor::createProject(const std::string& folder, const std::string& name) {
    std::string err;
    if (!vault::createVault(folder, name, project, &err)) {
        setStatus(err.empty() ? "Projekt konnte nicht erstellt werden." : err, true);
        return false;
    }
    undoStack_.clear();
    redoStack_.clear();
    watcher.reset(project);
    theme::settings().lastVault = folder;
    theme::settings().lastProjectName = name;
    theme::save();
    setStatus("Projekt erstellt: " + name);
    return true;
}

bool Editor::openProject(const std::string& folder) {
    std::string err;
    Project loadedProject;
    if (!vault::load(folder, loadedProject, &err)) {
        setStatus(err.empty() ? "Projekt konnte nicht geladen werden." : err, true);
        return false;
    }
    project = loadedProject;
    undoStack_.clear();
    redoStack_.clear();
    selection = Selection{};
    watcher.reset(project);
    theme::settings().lastVault = folder;
    theme::settings().lastProjectName = project.name;
    theme::save();
    setStatus("Projekt geoeffnet: " + project.name);
    return true;
}

void Editor::closeProject() {
    flushSaves();
    project.clear();
    undoStack_.clear();
    redoStack_.clear();
    selection = Selection{};
    theme::settings().lastVault.clear();
    theme::save();
    setStatus("Projekt geschlossen.");
}

void Editor::saveEverything() {
    if (!project.loaded) return;
    std::string err;
    if (vault::saveAll(project, &err)) {
        for (const Element& el : project.elements) watcher.touch(project, el);
        setStatus("Gespeichert nach " + project.vaultPath);
    } else {
        setStatus(err, true);
    }
}

void Editor::deleteElementWithFile(const std::string& id) {
    Element* el = project.element(id);
    if (!el) return;
    vault::deleteElementFile(project, *el);
    watcher.forget(el->filePath);
    project.removeElement(id);
    if (selection.is(SelKind::Element, id)) selection = Selection{};
    markAllElements();
    markActions();
    markConnections();
}

void Editor::renameElement(const std::string& id, const std::string& newName) {
    Element* el = project.element(id);
    if (!el || newName.empty()) return;
    watcher.forget(el->filePath);
    el->name = newName;
    auto it = el->values.find("name");
    if (it != el->values.end()) it->second = newName;
    markElement(id);
    markAllElements();
}

void Editor::moveElement(const std::string& id, const std::string& newGroupId) {
    Element* el = project.element(id);
    if (!el || !project.group(newGroupId)) return;
    vault::deleteElementFile(project, *el);
    watcher.forget(el->filePath);
    el->groupId = newGroupId;
    el->filePath.clear();
    project.syncElementFields(*el);
    markElement(id);
    markMetadata();
}

// ----------------------------------------------------------------- menu bar
void Editor::drawMainMenuBar() {
    AppSettings& s = theme::settings();
    if (!ImGui::BeginMainMenuBar()) return;

    if (ImGui::BeginMenu("Datei")) {
        if (ImGui::MenuItem("Neues Projekt...", "Strg+Shift+N")) dialogs::openNewProject(*this);
        if (ImGui::MenuItem("Projekt oeffnen...", "Strg+O")) {
            std::string folder = platform::pickFolder("Obsidian-Vault waehlen");
            if (!folder.empty()) openProject(folder);
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Speichern", "Strg+S", false, project.loaded)) saveEverything();
        if (ImGui::MenuItem("Vault im Explorer oeffnen", nullptr, false, project.loaded))
            platform::openInShell(project.vaultPath);
        if (ImGui::MenuItem("Projekt schliessen", nullptr, false, project.loaded)) closeProject();
        ImGui::Separator();
        if (ImGui::MenuItem("Beenden", "Alt+F4")) quitRequested = true;
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Bearbeiten")) {
        if (ImGui::MenuItem(canUndo() ? ("Rueckgaengig: " + undoLabel()).c_str() : "Rueckgaengig",
                            "Strg+Z", false, canUndo()))
            undo();
        if (ImGui::MenuItem(canRedo() ? ("Wiederholen: " + redoLabel()).c_str() : "Wiederholen",
                            "Strg+Y", false, canRedo()))
            redo();
        ImGui::Separator();
        if (ImGui::MenuItem("Neue Gruppe...", "Strg+G", false, project.loaded))
            dialogs::openNewGroup(*this, selection.kind == SelKind::Group ? selection.id : "");
        if (ImGui::MenuItem("Neues Element...", "Strg+N", false, project.loaded)) {
            std::string groupId = selection.kind == SelKind::Group ? selection.id : "";
            if (groupId.empty() && selection.kind == SelKind::Element) {
                if (const Element* el = project.element(selection.id)) groupId = el->groupId;
            }
            if (groupId.empty() && !project.groups.empty()) groupId = project.groups.front().id;
            dialogs::openNewElement(*this, groupId);
        }
        if (ImGui::MenuItem("Neue Aktion...", "Strg+T", false, project.loaded))
            dialogs::openNewAction(*this, 0, selection.kind == SelKind::Element ? selection.id : "");
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Ansicht")) {
        if (ImGui::BeginMenu("Design")) {
            if (ImGui::MenuItem("Hell", nullptr, s.preset == ThemePreset::Light)) {
                theme::applyPreset(ThemePreset::Light);
                theme::applyImGuiStyle();
                theme::save();
                setStatus("Helles Design aktiv.");
            }
            if (ImGui::MenuItem("Dunkel", nullptr, s.preset == ThemePreset::Dark)) {
                theme::applyPreset(ThemePreset::Dark);
                theme::applyImGuiStyle();
                theme::save();
                setStatus("Dunkles Design aktiv.");
            }
            ImGui::EndMenu();
        }
        ImGui::MenuItem("Einstellungen / Farben", nullptr, &s.showSettings);
        if (ImGui::MenuItem("Layout zuruecksetzen")) {
            ImGui::ClearIniSettings();
            resetLayoutRequested = true;
        }
        ImGui::Separator();
        ImGui::SliderFloat("Zeilenhoehe Timeline", &s.timelineTrackHeight, 20.0f, 80.0f, "%.0f px");
        ImGui::Checkbox("Leere Zeitraeume komprimieren", &s.timelineCompressGaps);
        ImGui::Checkbox("Automatisch speichern", &s.autosave);
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Fenster")) {
        ImGui::MenuItem("Timeline", nullptr, &s.showTimeline);
        ImGui::MenuItem("Gruppen-Manager", nullptr, &s.showGroups);
        ImGui::MenuItem("Details", nullptr, &s.showDetails);
        ImGui::MenuItem("Aktionen-Panel", nullptr, &s.showActions);
        ImGui::MenuItem("Story Visualizer", nullptr, &s.showStory);
        ImGui::MenuItem("Connections", nullptr, &s.showConnections);
        ImGui::MenuItem("Dateimanager", nullptr, &s.showFiles);
        ImGui::Separator();
        ImGui::MenuItem("ImGui Demo", nullptr, &s.showDemo);
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Hilfe")) {
        if (ImGui::MenuItem("Tastenkuerzel")) dialogs.shortcuts = true;
        if (ImGui::MenuItem("Ueber Story Editor")) dialogs.about = true;
        ImGui::EndMenu();
    }

    // right aligned status / project info
    std::string info = project.loaded ? project.name + "  |  " + project.vaultPath : "Kein Projekt";
    if (!status_.empty() && statusTimer_ > 0.0f) info = status_;
    float width = ImGui::CalcTextSize(info.c_str()).x;
    float avail = ImGui::GetWindowWidth() - width - 20.0f;
    if (avail > ImGui::GetCursorPosX()) {
        ImGui::SetCursorPosX(avail);
        ImVec4 col = (!status_.empty() && statusTimer_ > 0.0f)
                         ? (statusError_ ? theme::colors().errorColor : theme::colors().successColor)
                         : theme::colors().textSecondary;
        ImGui::PushStyleColor(ImGuiCol_Text, col);
        ImGui::TextUnformatted(info.c_str());
        ImGui::PopStyleColor();
    }
    ImGui::EndMainMenuBar();
}

void Editor::handleShortcuts() {
    ImGuiIO& io = ImGui::GetIO();
    if (!io.KeyCtrl) return;
    if (ImGui::IsKeyPressed(ImGuiKey_S, false)) saveEverything();
    if (ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
        if (io.KeyShift)
            redo();
        else
            undo();
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Y, false)) redo();
    if (!project.loaded) return;
    if (ImGui::IsKeyPressed(ImGuiKey_N, false)) {
        if (io.KeyShift) {
            dialogs::openNewProject(*this);
        } else {
            std::string groupId = selection.kind == SelKind::Group ? selection.id : "";
            if (groupId.empty() && selection.kind == SelKind::Element) {
                if (const Element* el = project.element(selection.id)) groupId = el->groupId;
            }
            if (groupId.empty() && !project.groups.empty()) groupId = project.groups.front().id;
            dialogs::openNewElement(*this, groupId);
        }
    }
    if (ImGui::IsKeyPressed(ImGuiKey_G, false))
        dialogs::openNewGroup(*this, selection.kind == SelKind::Group ? selection.id : "");
    if (ImGui::IsKeyPressed(ImGuiKey_T, false))
        dialogs::openNewAction(*this, 0, selection.kind == SelKind::Element ? selection.id : "");
}

void Editor::drawWindows() {
    AppSettings& s = theme::settings();
    if (s.showTimeline) drawTimelineWindow(*this, &s.showTimeline);
    if (s.showGroups) drawGroupManagerWindow(*this, &s.showGroups);
    if (s.showDetails) drawDetailsWindow(*this, &s.showDetails);
    if (s.showActions) drawActionsWindow(*this, &s.showActions);
    if (s.showStory) drawStoryVisualizerWindow(*this, &s.showStory);
    if (s.showConnections) drawConnectionsWindow(*this, &s.showConnections);
    if (s.showFiles) drawFileManagerWindow(*this, &s.showFiles);
    if (s.showSettings) drawSettingsWindow(*this, &s.showSettings);
    if (s.showDemo) ImGui::ShowDemoWindow(&s.showDemo);
    dialogs::draw(*this);
}

// -------------------------------------------------------- settings window
namespace {

enum class ListKind { ActionType, ConnectionType, Storyline };

int countUsage(Editor& ed, ListKind kind, const std::string& value) {
    int count = 0;
    if (kind == ListKind::ConnectionType) {
        for (const Connection& c : ed.project.connections) {
            if (c.type == value) ++count;
        }
        return count;
    }
    for (const Action& a : ed.project.actions) {
        if (kind == ListKind::ActionType ? a.type == value : a.storyline == value) ++count;
    }
    return count;
}

void replaceUsage(Editor& ed, ListKind kind, const std::string& from, const std::string& to) {
    if (kind == ListKind::ConnectionType) {
        for (Connection& c : ed.project.connections) {
            if (c.type == from) c.type = to;
        }
        ed.markConnections();
        return;
    }
    for (Action& a : ed.project.actions) {
        if (kind == ListKind::ActionType && a.type == from) a.type = to;
        if (kind == ListKind::Storyline && a.storyline == from) a.storyline = to;
    }
    ed.markActions();
}

// Add / rename / delete for one of the user owned lists.
void drawListEditor(Editor& ed, const char* title, std::vector<std::string>& items, ListKind kind) {
    ImGui::PushID(title);
    if (ImGui::TreeNodeEx(title, ImGuiTreeNodeFlags_DefaultOpen)) {
        int removeIndex = -1;
        if (!ImGui::BeginTable("entries", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {
            ImGui::TreePop();
            ImGui::PopID();
            return;
        }
        ImGui::TableSetupColumn("##name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("##usage", ImGuiTableColumnFlags_WidthFixed, 66.0f);
        ImGui::TableSetupColumn("##actions", ImGuiTableColumnFlags_WidthFixed, 96.0f);

        for (size_t i = 0; i < items.size(); ++i) {
            const std::string item = items[i];
            const int usage = countUsage(ed, kind, item);
            ImGui::PushID(static_cast<int>(i));
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(item.c_str());
            ImGui::TableSetColumnIndex(1);
            ImGui::PushStyleColor(ImGuiCol_Text, theme::colors().textSecondary);
            ImGui::Text("%dx", usage);
            ImGui::PopStyleColor();
            ui::tooltip("So oft wird der Eintrag im Projekt verwendet.");
            ImGui::TableSetColumnIndex(2);
            std::vector<std::string>* list = &items;
            if (ImGui::SmallButton("Umben.")) {
                dialogs::prompt(ed, "Eintrag umbenennen", title, item,
                                [&ed, kind, list, item](const std::string& value) {
                                    std::string name = trim(value);
                                    if (name.empty() || name == item) return;
                                    auto it = std::find(list->begin(), list->end(), item);
                                    if (it == list->end()) return;
                                    ed.pushUndo("Eintrag umbenannt");
                                    *it = name;
                                    replaceUsage(ed, kind, item, name);
                                    ed.markMetadata();
                                });
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("X")) {
                if (usage == 0) {
                    removeIndex = static_cast<int>(i);
                } else {
                    std::string fallback;
                    for (const std::string& other : items) {
                        if (other != item) {
                            fallback = other;
                            break;
                        }
                    }
                    if (kind == ListKind::Storyline) fallback.clear();
                    dialogs::confirm(
                        ed, "Eintrag loeschen", "\"" + item + "\" loeschen?",
                        std::to_string(usage) + " Eintraege benutzen ihn und werden auf \"" +
                            (fallback.empty() ? std::string("<keine>") : fallback) + "\" gesetzt.",
                        [&ed, kind, list, item, fallback]() {
                            ed.pushUndo("Eintrag geloescht");
                            replaceUsage(ed, kind, item, fallback);
                            list->erase(std::remove(list->begin(), list->end(), item), list->end());
                            ed.markMetadata();
                        });
                }
            }
            ui::tooltip("Eintrag loeschen");
            ImGui::PopID();
        }
        ImGui::EndTable();
        if (removeIndex >= 0) {
            ed.pushUndo("Eintrag geloescht");
            items.erase(items.begin() + removeIndex);
            ed.markMetadata();
        }

        static std::map<std::string, std::string> drafts;
        std::string& draft = drafts[title];
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 46.0f);
        bool submitted = ImGui::InputTextWithHint("##new", "Neuer Eintrag...", &draft,
                                                  ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine();
        if (ImGui::Button("+")) submitted = true;
        ui::tooltip("Eigenen Eintrag hinzufuegen (Enter geht auch)");
        if (submitted) {
            std::string value = trim(draft);
            if (!value.empty() && std::find(items.begin(), items.end(), value) == items.end()) {
                ed.pushUndo("Eintrag hinzugefuegt");
                items.push_back(value);
                ed.markMetadata();
            }
            draft.clear();
        }
        ImGui::TreePop();
    }
    ImGui::PopID();
}

}  // namespace

void drawSettingsWindow(Editor& ed, bool* open) {
    if (!ImGui::Begin("Einstellungen", open)) {
        ImGui::End();
        return;
    }
    AppSettings& s = theme::settings();
    ColorScheme& c = s.colors;

    if (ImGui::CollapsingHeader("Farben", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::TextUnformatted("Design");
        bool lightActive = s.preset == ThemePreset::Light;
        if (ImGui::RadioButton("Hell", lightActive)) {
            theme::applyPreset(ThemePreset::Light);
            theme::applyImGuiStyle();
            theme::save();
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Dunkel", !lightActive)) {
            theme::applyPreset(ThemePreset::Dark);
            theme::applyImGuiStyle();
            theme::save();
        }
        ImGui::SameLine();
        ui::helpMarker(
            "Setzt alle Farben auf das gewaehlte Design zurueck. Einzelne Farben lassen sich "
            "darunter weiter anpassen.");
        ImGui::Separator();

        struct Entry {
            const char* label;
            ImVec4* color;
        };
        Entry entries[] = {
            {"Text primaer", &c.textPrimary},
            {"Text sekundaer", &c.textSecondary},
            {"Hintergrund", &c.backgroundColor},
            {"Panel", &c.panelBackground},
            {"Akzent", &c.accentColor},
            {"Warnung", &c.warningColor},
            {"Erfolg", &c.successColor},
            {"Fehler", &c.errorColor},
            {"Timeline Hintergrund", &c.timelineBackground},
            {"Timeline Raster", &c.timelineGrid},
            {"Timeline Spur (alternierend)", &c.timelineTrackAlt},
            {"Timeline Lineal", &c.timelineRuler},
            {"Auswahl", &c.selectionColor},
            {"Hover", &c.hoverColor},
            {"Ghost (Drag)", &c.ghostColor},
            {"Verbindungslinie", &c.edgeColor},
            {"Knoten-Umriss", &c.nodeOutline},
            {"Block", &c.blockColor},
        };
        bool changed = false;
        for (Entry& e : entries) {
            if (ImGui::ColorEdit4(e.label, reinterpret_cast<float*>(e.color),
                                  ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar))
                changed = true;
        }
        if (changed) {
            theme::applyImGuiStyle();
            theme::save();
        }
        if (ImGui::Button("Design neu laden")) {
            theme::applyPreset(s.preset);
            theme::applyImGuiStyle();
            theme::save();
            ed.setStatus(std::string("Design \"") + theme::presetName(s.preset) + "\" zurueckgesetzt.");
        }
        ui::tooltip("Verwirft eigene Farbaenderungen und stellt das gewaehlte Design wieder her.");
    }

    if (ImGui::CollapsingHeader("Gruppen-Farben", ImGuiTreeNodeFlags_DefaultOpen)) {
        for (Group& g : ed.project.groups) {
            ImGui::PushID(g.id.c_str());
            ImVec4 col = ed.project.groupColor(g.id);
            if (ImGui::ColorEdit4(ed.project.groupPath(g.id).c_str(), reinterpret_cast<float*>(&col),
                                  ImGuiColorEditFlags_NoInputs)) {
                ed.pushUndo("Gruppenfarbe");
                g.color = col;
                g.colorExplicit = true;
                ed.markMetadata();
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Auto")) {
                ed.pushUndo("Gruppenfarbe automatisch");
                g.colorExplicit = false;
                ed.markMetadata();
            }
            ImGui::PopID();
        }
    }

    if (ImGui::CollapsingHeader("Eigene Listen", ImGuiTreeNodeFlags_DefaultOpen)) {
        ui::textSecondary(
            "Aktions-Typen, Beziehungstypen und Handlungsstraenge gehoeren dir - hier oder direkt "
            "im jeweiligen Dropdown anlegen.");
        drawListEditor(ed, "Aktions-Typen", ed.project.actionTypes, ListKind::ActionType);
        drawListEditor(ed, "Beziehungstypen", ed.project.connectionTypes, ListKind::ConnectionType);
        drawListEditor(ed, "Handlungsstraenge", ed.project.storylines, ListKind::Storyline);
    }

    if (ImGui::CollapsingHeader("Timeline")) {
        ImGui::SliderFloat("Spurhoehe", &s.timelineTrackHeight, 20.0f, 80.0f, "%.0f px");
        ImGui::SliderFloat("Breite Spurtitel", &s.timelineHeaderWidth, 120.0f, 420.0f, "%.0f px");
        ImGui::SliderFloat("Min. Abstand", &s.timelineMinGapPx, 4.0f, 120.0f, "%.0f px");
        ImGui::SliderFloat("Max. Abstand", &s.timelineMaxGapPx, 60.0f, 900.0f, "%.0f px");
        ImGui::Checkbox("Leere Zeitraeume komprimieren", &s.timelineCompressGaps);
    }

    if (ImGui::CollapsingHeader("Allgemein")) {
        ImGui::Checkbox("Automatisch speichern", &s.autosave);
        ImGui::TextUnformatted("Schriftgroesse wirkt nach Neustart:");
        ImGui::SliderFloat("##font", &s.fontSize, 12.0f, 28.0f, "%.0f px");
        if (ImGui::Button("Einstellungen speichern")) {
            theme::save();
            ed.setStatus("Einstellungen gespeichert.");
        }
    }
    ImGui::End();
}

}  // namespace se
