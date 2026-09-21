// Implementation of every modal dialog.
#include "ui/Dialogs.h"

#include <algorithm>
#include <filesystem>

#include "imgui.h"
#include "imgui_stdlib.h"

#include "app/Platform.h"
#include "core/StoryTime.h"
#include "core/VaultIO.h"
#include "ui/Editor.h"
#include "ui/Theme.h"
#include "ui/UiCommon.h"

namespace fs = std::filesystem;

#ifndef SE_VERSION
#define SE_VERSION "dev"
#endif

namespace se::dialogs {
namespace {

void openModal(const char* title, bool open) {
    if (open && !ImGui::IsPopupOpen(title)) ImGui::OpenPopup(title);
    // keep dialogs centred on the window they belong to
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
}

// --------------------------------------------------------------- project
void drawNewProject(Editor& ed) {
    NewProjectDialog& st = ed.dialogs.newProject;
    openModal("Neues Projekt", st.open);
    if (!ImGui::BeginPopupModal("Neues Projekt", &st.open, ImGuiWindowFlags_AlwaysAutoResize)) return;

    ImGui::TextUnformatted("Projektname");
    ImGui::SetNextItemWidth(360.0f);
    ImGui::InputText("##name", &st.name);
    ImGui::TextUnformatted("Speicherort (Obsidian-Vault)");
    ImGui::SetNextItemWidth(360.0f);
    ImGui::InputText("##folder", &st.folder);
    ImGui::SameLine();
    if (ImGui::Button("Waehlen...")) {
        std::string picked = platform::pickFolder("Ordner fuer den Vault waehlen");
        if (!picked.empty()) st.folder = picked;
    }
    ui::textSecondary("Es wird die Standard-Struktur (Characters, Locations, Objects, ...) angelegt.");

    ImGui::Separator();
    bool valid = !st.name.empty() && !st.folder.empty();
    if (!valid) ImGui::BeginDisabled();
    if (ImGui::Button("Erstellen", ImVec2(120, 0))) {
        std::string folder = st.folder;
        std::error_code ec;
        if (fs::exists(platform::fsPath(folder + "/metadata.json"), ec)) {
            ed.setStatus("In diesem Ordner liegt bereits ein Projekt - es wird geoeffnet.", true);
            ed.openProject(folder);
        } else {
            ed.createProject(folder, st.name);
        }
        st.open = false;
        ImGui::CloseCurrentPopup();
    }
    if (!valid) ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Abbrechen", ImVec2(120, 0))) {
        st.open = false;
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

// ----------------------------------------------------------------- group
void drawGroupDialog(Editor& ed) {
    GroupDialog& st = ed.dialogs.group;
    const char* title = st.isNew ? "Neue Gruppe" : "Gruppe bearbeiten";
    openModal(title, st.open);
    if (!ImGui::BeginPopupModal(title, &st.open, ImGuiWindowFlags_AlwaysAutoResize)) return;

    ImGui::TextUnformatted("Name");
    ImGui::SetNextItemWidth(320.0f);
    ImGui::InputText("##name", &st.name);

    ImGui::TextUnformatted("Uebergeordnete Gruppe");
    ImGui::SetNextItemWidth(320.0f);
    ui::groupCombo(ed, "##parent", st.parentId, true, st.isNew ? std::string() : st.groupId);

    ImGui::Checkbox("Eigene Farbe", &st.explicitColor);
    if (st.explicitColor) {
        ImGui::SameLine();
        ImGui::ColorEdit4("##color", reinterpret_cast<float*>(&st.color),
                          ImGuiColorEditFlags_NoInputs);
    } else {
        ImGui::SameLine();
        ui::textSecondary("Farbe wird automatisch aus der Elterngruppe abgeleitet.");
    }

    ImGui::Separator();
    bool valid = !st.name.empty();
    if (!valid) ImGui::BeginDisabled();
    if (ImGui::Button("Speichern", ImVec2(120, 0))) {
        if (st.isNew) {
            ed.pushUndo("Gruppe erstellt");
            Group& g = ed.project.addGroup(st.name, st.parentId);
            g.color = st.color;
            g.colorExplicit = st.explicitColor;
            ed.select(SelKind::Group, g.id);
        } else if (Group* g = ed.project.group(st.groupId)) {
            ed.pushUndo("Gruppe bearbeitet");
            g->name = st.name;
            if (!ed.project.isAncestorGroup(st.groupId, st.parentId)) g->parentId = st.parentId;
            g->color = st.color;
            g->colorExplicit = st.explicitColor;
            ed.markAllElements();
        }
        ed.markMetadata();
        std::string err;
        vault::ensureGroupDirs(ed.project, &err);
        st.open = false;
        ImGui::CloseCurrentPopup();
    }
    if (!valid) ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Abbrechen", ImVec2(120, 0))) {
        st.open = false;
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

// --------------------------------------------------------------- element
void drawElementDialog(Editor& ed) {
    ElementDialog& st = ed.dialogs.element;
    const char* title = st.isNew ? "Neues Element" : "Element bearbeiten";
    openModal(title, st.open);
    if (!ImGui::BeginPopupModal(title, &st.open, ImGuiWindowFlags_AlwaysAutoResize)) return;

    ui::textSecondary(("Gruppe: " + ed.project.groupPath(st.groupId)).c_str());
    ImGui::TextUnformatted("Name");
    ImGui::SetNextItemWidth(380.0f);
    ImGui::InputText("##name", &st.name);
    ImGui::Separator();

    std::vector<FieldDef> fields = ed.project.effectiveFields(st.groupId);
    ImGui::BeginChild("fields", ImVec2(460, std::min(420.0f, 60.0f + 62.0f * fields.size())));
    for (const FieldDef& f : fields) {
        ImGui::PushID(f.name.c_str());
        ImGui::TextUnformatted(f.name.c_str());
        if (f.required) {
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, theme::colors().warningColor);
            ImGui::TextUnformatted("* Pflicht");
            ImGui::PopStyleColor();
        }
        std::string& value = st.values[f.name];
        ui::fieldValueEditor(ed, f, value, f.name.c_str(), st.groupId);
        ImGui::PopID();
        ImGui::Spacing();
    }
    if (fields.empty()) ui::textSecondary("Diese Gruppe hat noch kein Template.");
    ImGui::EndChild();

    if (!st.error.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, theme::colors().errorColor);
        ImGui::TextUnformatted(st.error.c_str());
        ImGui::PopStyleColor();
    }

    ImGui::Separator();
    if (ImGui::Button("Speichern", ImVec2(120, 0))) {
        st.error.clear();
        if (st.name.empty()) st.error = "Name darf nicht leer sein.";
        for (const FieldDef& f : fields) {
            if (f.required && trim(st.values[f.name]).empty())
                st.error = "Pflichtfeld \"" + f.name + "\" ist leer.";
        }
        if (st.error.empty()) {
            if (st.isNew) {
                ed.pushUndo("Element erstellt");
                Element& el = ed.project.addElement(st.name, st.groupId);
                for (const auto& kv : st.values) el.values[kv.first] = kv.second;
                if (el.values.count("name")) el.values["name"] = st.name;
                ed.project.syncElementFields(el);
                ed.markElement(el.id);
                ed.select(SelKind::Element, el.id);
                ed.focusElementId = el.id;
            } else if (Element* el = ed.project.element(st.elementId)) {
                ed.pushUndo("Element bearbeitet");
                for (const auto& kv : st.values) el->values[kv.first] = kv.second;
                if (el->name != st.name) ed.renameElement(el->id, st.name);
                if (el->values.count("name")) el->values["name"] = st.name;
                ed.project.syncElementFields(*el);
                ed.markElement(el->id);
            }
            st.open = false;
            ImGui::CloseCurrentPopup();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Abbrechen", ImVec2(120, 0))) {
        st.open = false;
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

// -------------------------------------------------------------- template
void drawFieldDialog(Editor& ed) {
    FieldDialog& st = ed.dialogs.field;
    const char* title = st.isNew ? "Neues Feld" : "Feld bearbeiten";
    openModal(title, st.open);
    if (!ImGui::BeginPopupModal(title, &st.open, ImGuiWindowFlags_AlwaysAutoResize)) return;

    ImGui::TextUnformatted("Feldname");
    ImGui::SetNextItemWidth(320.0f);
    ImGui::InputTextWithHint("##name", "z.B. age, birthday, status", &st.field.name);

    int typeCount = 0;
    const char* const* labels = fieldTypeLabels(&typeCount);
    int typeIndex = static_cast<int>(st.field.type);
    ImGui::TextUnformatted("Typ");
    ImGui::SetNextItemWidth(320.0f);
    if (ImGui::Combo("##type", &typeIndex, labels, typeCount))
        st.field.type = static_cast<FieldType>(typeIndex);

    ImGui::Checkbox("Pflichtfeld", &st.field.required);
    ui::helpMarker("Pflichtfelder muessen beim Anlegen eines Elements ausgefuellt werden.");

    ImGui::TextUnformatted("Standardwert");
    ImGui::SetNextItemWidth(320.0f);
    ImGui::InputText("##default", &st.field.defaultValue);

    ImGui::TextUnformatted("Beschreibung / Hilfetext");
    ImGui::InputTextMultiline("##desc", &st.field.description, ImVec2(320, 54));

    ImGui::TextUnformatted("Anzeigeformat (optional)");
    ImGui::SetNextItemWidth(320.0f);
    ImGui::InputTextWithHint("##format", "z.B. DD.MM.YYYY", &st.field.displayFormat);

    if (st.field.type == FieldType::Enum) {
        ImGui::TextUnformatted("Auswahl-Optionen (eine pro Zeile)");
        ImGui::InputTextMultiline("##enum", &st.enumOptions, ImVec2(320, 80));
    }

    if (!st.error.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, theme::colors().errorColor);
        ImGui::TextUnformatted(st.error.c_str());
        ImGui::PopStyleColor();
    }

    ImGui::Separator();
    if (ImGui::Button("Speichern", ImVec2(120, 0))) {
        st.error.clear();
        st.field.name = trim(st.field.name);
        if (st.field.name.empty()) st.error = "Feldname darf nicht leer sein.";
        TemplateDialog& tpl = ed.dialogs.templateEditor;
        for (size_t i = 0; i < tpl.fields.size(); ++i) {
            if (static_cast<int>(i) == st.index) continue;
            if (tpl.fields[i].name == st.field.name) st.error = "Feldname existiert bereits.";
        }
        if (st.error.empty()) {
            st.field.enumOptions.clear();
            for (const std::string& line : splitString(st.enumOptions, '\n')) {
                std::string t = trim(line);
                if (!t.empty()) st.field.enumOptions.push_back(t);
            }
            if (st.isNew)
                tpl.fields.push_back(st.field);
            else if (st.index >= 0 && st.index < static_cast<int>(tpl.fields.size()))
                tpl.fields[st.index] = st.field;
            st.open = false;
            ImGui::CloseCurrentPopup();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Abbrechen", ImVec2(120, 0))) {
        st.open = false;
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

void drawTemplateDialog(Editor& ed) {
    TemplateDialog& st = ed.dialogs.templateEditor;
    std::string title = "Template: " + ed.project.groupPath(st.groupId);
    openModal(title.c_str(), st.open);
    if (ImGui::BeginPopupModal(title.c_str(), &st.open, ImGuiWindowFlags_AlwaysAutoResize)) {
        std::vector<FieldDef> inherited;
        if (const Group* g = ed.project.group(st.groupId)) {
            for (const FieldDef& f : ed.project.effectiveFields(g->parentId)) inherited.push_back(f);
        }
        if (!inherited.empty()) {
            ImGui::TextUnformatted("Geerbte Felder:");
            for (const FieldDef& f : inherited) {
                ImGui::BulletText("%s  [%s]%s", f.name.c_str(), fieldTypeName(f.type),
                                  f.required ? "  Pflicht" : "");
            }
            ImGui::Separator();
        }

        ImGui::TextUnformatted("Eigene Felder:");
        int removeIndex = -1;
        for (size_t i = 0; i < st.fields.size(); ++i) {
            FieldDef& f = st.fields[i];
            ImGui::PushID(static_cast<int>(i));
            ImGui::Text("%-18s [Typ: %-9s] [Default: %s] %s", f.name.c_str(), fieldTypeName(f.type),
                        f.defaultValue.empty() ? "\"\"" : f.defaultValue.c_str(),
                        f.required ? "[Pflicht]" : "");
            ImGui::SameLine();
            if (ImGui::SmallButton("Bearbeiten")) {
                FieldDialog& fd = ed.dialogs.field;
                fd.open = true;
                fd.isNew = false;
                fd.index = static_cast<int>(i);
                fd.field = f;
                fd.enumOptions = joinList(f.enumOptions, "\n");
                fd.error.clear();
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Loeschen")) removeIndex = static_cast<int>(i);
            ImGui::PopID();
        }
        if (removeIndex >= 0) st.fields.erase(st.fields.begin() + removeIndex);
        if (st.fields.empty()) ui::textSecondary("Noch keine eigenen Felder.");

        if (ImGui::Button("+ Neues Feld")) {
            FieldDialog& fd = ed.dialogs.field;
            fd.open = true;
            fd.isNew = true;
            fd.index = -1;
            fd.field = FieldDef{};
            fd.enumOptions.clear();
            fd.error.clear();
        }

        ImGui::Separator();
        ui::textSecondary(
            "Speichern ergaenzt fehlende Felder bei allen Elementen dieser Gruppe (inkl. Untergruppen).");
        if (ImGui::Button("Alle speichern", ImVec2(140, 0))) {
            if (Group* g = ed.project.group(st.groupId)) {
                ed.pushUndo("Template gespeichert");
                g->fields = st.fields;
                ed.project.applyTemplateToElements(st.groupId);
                ed.markMetadata();
                ed.markAllElements();
                ed.setStatus("Template aktualisiert.");
            }
            st.open = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Abbrechen", ImVec2(120, 0))) {
            st.open = false;
            ImGui::CloseCurrentPopup();
        }
        drawFieldDialog(ed);
        ImGui::EndPopup();
    }
}

// ---------------------------------------------------------------- action
void drawActionDialog(Editor& ed) {
    ActionDialog& st = ed.dialogs.action;
    const char* title = st.isNew ? "Neue Aktion" : "Aktion bearbeiten";
    openModal(title, st.open);
    if (!ImGui::BeginPopupModal(title, &st.open, ImGuiWindowFlags_AlwaysAutoResize)) return;

    ImGui::TextUnformatted("Titel");
    ImGui::SetNextItemWidth(460.0f);
    ImGui::InputText("##title", &st.draft.title);

    bool listGrew = false;
    ImGui::SetNextItemWidth(220.0f);
    ui::editableCombo("Typ", ed.project.actionTypes, st.draft.type, true, &listGrew);
    ui::tooltip("Eigene Aktions-Typen koennen direkt im Dropdown angelegt werden.");
    ImGui::SameLine();
    ImGui::Checkbox("wichtig (Major)", &st.draft.major);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(160.0f);
    ui::editableCombo("Strang", ed.project.storylines, st.draft.storyline, true, &listGrew);
    ui::tooltip("Eigene Handlungsstraenge koennen direkt im Dropdown angelegt werden.");
    if (listGrew) ed.markMetadata();

    ImGui::TextUnformatted("Beschreibung / Szene");
    ImGui::InputTextMultiline("##desc", &st.draft.description, ImVec2(460, 120));

    ImGui::Separator();
    ImGui::Checkbox("Zeit relativ zu einer anderen Aktion", &st.draft.useRelative);
    if (st.draft.useRelative) {
        std::vector<std::string> titles;
        std::vector<std::string> ids;
        for (const Action& a : ed.project.actions) {
            if (a.id == st.draft.id) continue;
            titles.push_back(a.title);
            ids.push_back(a.id);
        }
        std::string current;
        for (size_t i = 0; i < ids.size(); ++i) {
            if (ids[i] == st.draft.relativeToId) current = titles[i];
        }
        ImGui::SetNextItemWidth(300.0f);
        if (ui::comboStrings("Nach", titles, current, true)) {
            st.draft.relativeToId.clear();
            for (size_t i = 0; i < titles.size(); ++i) {
                if (titles[i] == current) st.draft.relativeToId = ids[i];
            }
        }
        int minutes = static_cast<int>(st.draft.relativeOffset);
        ImGui::SetNextItemWidth(160.0f);
        if (ImGui::InputInt("Minuten danach", &minutes)) st.draft.relativeOffset = minutes;
        const Action* ref = ed.project.action(st.draft.relativeToId);
        if (ref)
            ui::textSecondary(("Ergibt: " + formatStoryTimeLong(ed.project.resolveActionTime(*ref) +
                                                               st.draft.relativeOffset))
                                  .c_str());
    } else {
        ImGui::TextUnformatted("Zeitpunkt");
        ImGui::SetNextItemWidth(220.0f);
        ImGui::InputTextWithHint("##time", "Tag 5, 14:00", &st.timeText);
        long long parsed = 0;
        ImGui::SameLine();
        if (parseStoryTime(st.timeText, &parsed)) {
            ImGui::PushStyleColor(ImGuiCol_Text, theme::colors().successColor);
            ImGui::TextUnformatted(formatStoryTimeLong(parsed).c_str());
            ImGui::PopStyleColor();
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text, theme::colors().warningColor);
            ImGui::TextUnformatted("nicht lesbar");
            ImGui::PopStyleColor();
        }
    }

    ImGui::Separator();
    ui::elementMultiSelect(ed, "Beteiligte Elemente (Charaktere, Orte, Objekte)", st.draft.elementIds,
                           st.elementSearch, 140.0f);

    ImGui::TextUnformatted("Tags (Komma getrennt, neue einfach eintippen)");
    ui::tagPicker(ed, "##tags", st.tagsText);

    if (ImGui::CollapsingHeader("Anhaenge")) {
        for (size_t i = 0; i < st.draft.attachments.size(); ++i) {
            ImGui::PushID(static_cast<int>(i));
            ImGui::TextUnformatted(st.draft.attachments[i].c_str());
            ImGui::SameLine();
            if (ImGui::SmallButton("Entfernen")) {
                st.draft.attachments.erase(st.draft.attachments.begin() + i);
                ImGui::PopID();
                break;
            }
            ImGui::PopID();
        }
        if (ImGui::Button("Datei hinzufuegen...")) {
            std::vector<std::string> picked = platform::pickFiles("Anhaenge waehlen");
            for (const std::string& file : picked) {
                std::error_code ec;
                fs::path src = platform::fsPath(file);
                std::string rel = "Assets/Images/" + platform::pathToUtf8(src.filename());
                platform::ensureDir(vault::absolutePath(ed.project, "Assets/Images"), nullptr);
                fs::copy_file(src, platform::fsPath(vault::absolutePath(ed.project, rel)),
                              fs::copy_options::overwrite_existing, ec);
                if (!ec) st.draft.attachments.push_back(rel);
            }
        }
        if (!ed.droppedFiles.empty()) {
            for (const std::string& file : ed.droppedFiles) {
                std::error_code ec;
                fs::path src = platform::fsPath(file);
                std::string rel = "Assets/Images/" + platform::pathToUtf8(src.filename());
                platform::ensureDir(vault::absolutePath(ed.project, "Assets/Images"), nullptr);
                fs::copy_file(src, platform::fsPath(vault::absolutePath(ed.project, rel)),
                              fs::copy_options::overwrite_existing, ec);
                if (!ec) st.draft.attachments.push_back(rel);
            }
            ed.droppedFiles.clear();
        }
    }

    if (ImGui::CollapsingHeader("Attribut-Aenderungen (Mutations)")) {
        int removeIndex = -1;
        for (size_t i = 0; i < st.draft.mutations.size(); ++i) {
            Mutation& m = st.draft.mutations[i];
            ImGui::PushID(static_cast<int>(i));
            ImGui::SetNextItemWidth(200.0f);
            if (ui::elementCombo(ed, "##el", m.elementId, false)) {
                m.field.clear();
                m.oldValue.clear();
            }
            ImGui::SameLine();
            std::vector<std::string> fieldNames;
            if (const Element* el = ed.project.element(m.elementId)) {
                for (const std::string& f : el->fieldOrder) fieldNames.push_back(f);
            }
            ImGui::SetNextItemWidth(130.0f);
            bool fieldAdded = false;
            if (ui::editableCombo("##field", fieldNames, m.field, false, &fieldAdded)) {
                Element* el = ed.project.element(m.elementId);
                if (el) {
                    // a field name typed here is created on the element right away
                    if (fieldAdded && el->values.find(m.field) == el->values.end()) {
                        el->values[m.field] = "";
                        el->fieldOrder.push_back(m.field);
                        ed.markElement(el->id);
                    }
                    auto it = el->values.find(m.field);
                    m.oldValue = it != el->values.end() ? it->second : std::string();
                }
            }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(90.0f);
            ImGui::InputText("##old", &m.oldValue);
            ImGui::SameLine();
            ImGui::TextUnformatted("->");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(90.0f);
            ImGui::InputText("##new", &m.newValue);
            ImGui::SameLine();
            if (ImGui::SmallButton("X")) removeIndex = static_cast<int>(i);
            ImGui::PopID();
        }
        if (removeIndex >= 0) st.draft.mutations.erase(st.draft.mutations.begin() + removeIndex);
        if (ImGui::Button("+ Aenderung")) {
            Mutation m;
            if (!st.draft.elementIds.empty()) m.elementId = st.draft.elementIds.front();
            st.draft.mutations.push_back(m);
        }
    }

    if (!st.error.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, theme::colors().errorColor);
        ImGui::TextUnformatted(st.error.c_str());
        ImGui::PopStyleColor();
    }

    ImGui::Separator();
    if (ImGui::Button("Speichern", ImVec2(120, 0))) {
        st.error.clear();
        if (trim(st.draft.title).empty()) st.error = "Titel darf nicht leer sein.";
        long long parsed = 0;
        if (!st.draft.useRelative && !parseStoryTime(st.timeText, &parsed))
            st.error = "Zeitpunkt nicht lesbar (z.B. \"Tag 5, 14:00\").";
        if (st.error.empty()) {
            st.draft.tags.clear();
            for (const std::string& part : splitString(st.tagsText, ',')) {
                std::string t = trim(part);
                if (!t.empty()) st.draft.tags.push_back(t);
            }
            if (!st.draft.useRelative) st.draft.time = parsed;
            if (st.draft.useRelative) {
                const Action* ref = ed.project.action(st.draft.relativeToId);
                if (ref) st.draft.time = ed.project.resolveActionTime(*ref) + st.draft.relativeOffset;
            }
            if (st.isNew) {
                ed.pushUndo("Aktion erstellt");
                st.draft.id = newId("act");
                ed.project.actions.push_back(st.draft);
                ed.select(SelKind::Action, st.draft.id);
            } else if (Action* a = ed.project.action(st.draft.id)) {
                ed.pushUndo("Aktion bearbeitet");
                *a = st.draft;
            }
            if (!st.draft.storyline.empty() &&
                std::find(ed.project.storylines.begin(), ed.project.storylines.end(),
                          st.draft.storyline) == ed.project.storylines.end()) {
                ed.project.storylines.push_back(st.draft.storyline);
                ed.markMetadata();
            }
            ed.markActions();
            st.open = false;
            ImGui::CloseCurrentPopup();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Abbrechen", ImVec2(120, 0))) {
        st.open = false;
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

// ------------------------------------------------------------ connection
void drawConnectionDialog(Editor& ed) {
    ConnectionDialog& st = ed.dialogs.connection;
    const char* title = st.isNew ? "Verbindung erstellen" : "Verbindung bearbeiten";
    openModal(title, st.open);
    if (!ImGui::BeginPopupModal(title, &st.open, ImGuiWindowFlags_AlwaysAutoResize)) return;

    ImGui::SetNextItemWidth(300.0f);
    ui::elementCombo(ed, "Von", st.draft.sourceId, false);
    ImGui::SetNextItemWidth(300.0f);
    ui::elementCombo(ed, "Nach", st.draft.targetId, false);

    bool typeAdded = false;
    ImGui::SetNextItemWidth(220.0f);
    ui::editableCombo("Typ", ed.project.connectionTypes, st.draft.type, false, &typeAdded);
    ui::tooltip("Eigene Beziehungstypen (z.B. mentor_of) koennen direkt im Dropdown angelegt werden.");
    if (typeAdded) ed.markMetadata();

    ImGui::TextUnformatted("Beschreibung");
    ImGui::InputTextMultiline("##desc", &st.draft.description, ImVec2(360, 70));
    ImGui::SetNextItemWidth(160.0f);
    ImGui::InputTextWithHint("Von (Zeit)", "Tag 1", &st.draft.startDate);
    ImGui::SetNextItemWidth(160.0f);
    ImGui::InputTextWithHint("Bis (Zeit)", "offen", &st.draft.endDate);

    {
        std::vector<std::string> names;
        for (const Block& b : ed.project.blocks) names.push_back(b.name);
        std::string current;
        for (const Block& b : ed.project.blocks) {
            if (b.id == st.draft.blockId) current = b.name;
        }
        bool blockAdded = false;
        ImGui::SetNextItemWidth(220.0f);
        if (ui::editableCombo("Block", names, current, true, &blockAdded)) {
            st.draft.blockId.clear();
            for (const Block& b : ed.project.blocks) {
                if (b.name == current) st.draft.blockId = b.id;
            }
            if (blockAdded && st.draft.blockId.empty() && !current.empty()) {
                // a name typed into the dropdown creates the block
                Block& created = ed.project.addBlock(current);
                st.draft.blockId = created.id;
                ed.markConnections();
            }
        }
        ui::tooltip("Bloecke fassen zusammengehoerende Verbindungen zusammen - hier auch neu anlegbar.");
    }

    ImGui::Separator();
    bool valid = !st.draft.sourceId.empty() && !st.draft.targetId.empty() &&
                 st.draft.sourceId != st.draft.targetId;
    if (!valid) ImGui::BeginDisabled();
    if (ImGui::Button("Speichern", ImVec2(120, 0))) {
        if (st.isNew) {
            ed.pushUndo("Verbindung erstellt");
            st.draft.id = newId("conn");
            ed.project.connections.push_back(st.draft);
        } else if (Connection* conn = ed.project.connection(st.draft.id)) {
            ed.pushUndo("Verbindung bearbeitet");
            *conn = st.draft;
        }
        ed.markConnections();
        st.open = false;
        ImGui::CloseCurrentPopup();
    }
    if (!valid) ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Abbrechen", ImVec2(120, 0))) {
        st.open = false;
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

void drawBlockDialog(Editor& ed) {
    BlockDialog& st = ed.dialogs.block;
    openModal("Block erstellen", st.open);
    if (!ImGui::BeginPopupModal("Block erstellen", &st.open, ImGuiWindowFlags_AlwaysAutoResize)) return;

    ImGui::TextUnformatted("Name");
    ImGui::SetNextItemWidth(300.0f);
    ImGui::InputText("##name", &st.draft.name);
    ImGui::TextUnformatted("Beschreibung");
    ImGui::InputTextMultiline("##desc", &st.draft.description, ImVec2(300, 70));

    ImGui::TextUnformatted("Enthaltene Verbindungen:");
    for (const std::string& id : st.connectionIds) {
        const Connection* conn = ed.project.connection(id);
        if (!conn) continue;
        ImGui::BulletText("%s %s %s", ed.project.displayName(conn->sourceId).c_str(),
                          conn->type.c_str(), ed.project.displayName(conn->targetId).c_str());
    }
    if (st.connectionIds.empty())
        ui::textSecondary("Keine Verbindungen zwischen den markierten Knoten gefunden.");

    ImGui::Separator();
    bool valid = !trim(st.draft.name).empty();
    if (!valid) ImGui::BeginDisabled();
    if (ImGui::Button("Speichern", ImVec2(120, 0))) {
        ed.pushUndo("Block erstellt");
        Block& b = ed.project.addBlock(st.draft.name);
        b.description = st.draft.description;
        for (const std::string& id : st.connectionIds) {
            if (Connection* conn = ed.project.connection(id)) conn->blockId = b.id;
        }
        ed.markConnections();
        st.open = false;
        ImGui::CloseCurrentPopup();
    }
    if (!valid) ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Abbrechen", ImVec2(120, 0))) {
        st.open = false;
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

// ------------------------------------------------------------ misc dialogs
void drawMoveDialog(Editor& ed) {
    MoveDialog& st = ed.dialogs.move;
    openModal("Element verschieben", st.open);
    if (!ImGui::BeginPopupModal("Element verschieben", &st.open, ImGuiWindowFlags_AlwaysAutoResize))
        return;
    const Element* el = ed.project.element(st.elementId);
    ui::textSecondary(el ? ed.project.elementPath(el->id).c_str() : "?");
    ImGui::SetNextItemWidth(320.0f);
    ui::groupCombo(ed, "Zielgruppe", st.targetGroupId, false);
    ImGui::Separator();
    bool valid = !st.targetGroupId.empty();
    if (!valid) ImGui::BeginDisabled();
    if (ImGui::Button("Verschieben", ImVec2(120, 0))) {
        ed.pushUndo("Element verschoben");
        ed.moveElement(st.elementId, st.targetGroupId);
        st.open = false;
        ImGui::CloseCurrentPopup();
    }
    if (!valid) ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Abbrechen", ImVec2(120, 0))) {
        st.open = false;
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

void drawConfirmDialog(Editor& ed) {
    ConfirmDialog& st = ed.dialogs.confirm;
    openModal(st.title.empty() ? "Bestaetigen" : st.title.c_str(), st.open);
    const char* title = st.title.empty() ? "Bestaetigen" : st.title.c_str();
    if (!ImGui::BeginPopupModal(title, &st.open, ImGuiWindowFlags_AlwaysAutoResize)) return;
    ImGui::TextWrapped("%s", st.message.c_str());
    if (!st.details.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, theme::colors().warningColor);
        ImGui::TextWrapped("%s", st.details.c_str());
        ImGui::PopStyleColor();
    }
    ImGui::Separator();
    ImGui::PushStyleColor(ImGuiCol_Button, theme::withAlpha(theme::colors().errorColor, 0.75f));
    if (ImGui::Button("Ja, ausfuehren", ImVec2(140, 0))) {
        if (st.onConfirm) st.onConfirm();
        st.open = false;
        st.onConfirm = nullptr;
        ImGui::CloseCurrentPopup();
    }
    ImGui::PopStyleColor();
    ImGui::SameLine();
    if (ImGui::Button("Abbrechen", ImVec2(120, 0))) {
        st.open = false;
        st.onConfirm = nullptr;
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

void drawPromptDialog(Editor& ed) {
    TextPromptDialog& st = ed.dialogs.prompt;
    const char* title = st.title.empty() ? "Eingabe" : st.title.c_str();
    openModal(title, st.open);
    if (!ImGui::BeginPopupModal(title, &st.open, ImGuiWindowFlags_AlwaysAutoResize)) return;
    ImGui::TextUnformatted(st.label.c_str());
    ImGui::SetNextItemWidth(320.0f);
    bool enter = ImGui::InputText("##value", &st.value, ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::Separator();
    if (ImGui::Button("OK", ImVec2(120, 0)) || enter) {
        if (st.onAccept) st.onAccept(st.value);
        st.open = false;
        st.onAccept = nullptr;
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Abbrechen", ImVec2(120, 0))) {
        st.open = false;
        st.onAccept = nullptr;
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

void drawConflictDialog(Editor& ed) {
    ConflictDialog& st = ed.dialogs.conflict;
    openModal("Externe Aenderung", st.open);
    if (!ImGui::BeginPopupModal("Externe Aenderung", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        return;

    Element* el = ed.project.element(st.change.elementId);
    ImGui::PushStyleColor(ImGuiCol_Text, theme::colors().warningColor);
    ImGui::TextWrapped("Die Datei \"%s\" wurde ausserhalb des Programms veraendert!",
                       st.change.relativePath.c_str());
    ImGui::PopStyleColor();
    ImGui::TextWrapped(
        "Die Aenderung kann inkonsistent mit den Programm-Daten sein. .md-Dateien sollten nur ueber "
        "die UI bearbeitet werden.");
    ImGui::Separator();
    ImGui::TextUnformatted("Inhalt auf der Festplatte:");
    ImGui::InputTextMultiline("##external", &st.externalText, ImVec2(560, 220),
                              ImGuiInputTextFlags_ReadOnly);
    ImGui::Separator();

    auto parseExternal = [&](std::vector<std::pair<std::string, std::string>>* fields,
                             std::string* name, std::string* body) {
        vault::parseElementMarkdown(st.externalText, name, fields, body, nullptr, nullptr);
    };

    if (ImGui::Button("Reload", ImVec2(120, 0))) {
        if (el) {
            ed.pushUndo("Externe Datei geladen");
            std::vector<std::pair<std::string, std::string>> fields;
            std::string name, body;
            parseExternal(&fields, &name, &body);
            el->values.clear();
            el->fieldOrder.clear();
            for (const auto& kv : fields) {
                el->values[kv.first] = kv.second;
                el->fieldOrder.push_back(kv.first);
            }
            if (!name.empty()) el->name = name;
            el->body = body;
            ed.project.syncElementFields(*el);
            ed.markElement(el->id);
            ed.setStatus("Externe Version uebernommen.");
        }
        st.open = false;
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    ui::tooltip("Verwirft die Daten im Programm und uebernimmt die Datei.");
    if (ImGui::Button("Merge", ImVec2(120, 0))) {
        if (el) {
            ed.pushUndo("Externe Datei zusammengefuehrt");
            std::vector<std::pair<std::string, std::string>> fields;
            std::string name, body;
            parseExternal(&fields, &name, &body);
            for (const auto& kv : fields) {
                el->values[kv.first] = kv.second;  // externe Werte gewinnen
                if (std::find(el->fieldOrder.begin(), el->fieldOrder.end(), kv.first) ==
                    el->fieldOrder.end())
                    el->fieldOrder.push_back(kv.first);
            }
            if (!body.empty()) el->body = body;
            ed.project.syncElementFields(*el);
            ed.markElement(el->id);
            ed.setStatus("Zusammengefuehrt.");
        }
        st.open = false;
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    ui::tooltip("Uebernimmt externe Feldwerte, behaelt zusaetzliche Programm-Felder.");
    if (ImGui::Button("Cancel", ImVec2(120, 0))) {
        if (el) {
            ed.markElement(el->id);  // schreibt die interne Version zurueck
            ed.setStatus("Externe Aenderung verworfen.");
        }
        st.open = false;
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    ui::tooltip("Ignoriert die externe Aenderung und schreibt die Programm-Daten zurueck.");
    ImGui::EndPopup();
}

void drawAbout(Editor& ed) {
    openModal("Ueber Story Editor", ed.dialogs.about);
    if (ImGui::BeginPopupModal("Ueber Story Editor", &ed.dialogs.about,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("Story Editor " SE_VERSION);
        ui::textSecondary("Schreib-Management fuer komplexe Geschichten.");
        ImGui::Separator();
        ImGui::TextUnformatted("C++17 + Dear ImGui (Win32/DirectX11)");
        ImGui::TextUnformatted("Speicherung: Obsidian-Vault (Markdown + JSON)");
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, theme::colors().warningColor);
        ImGui::TextWrapped(
            "Wichtig: .md-Dateien werden ausschliesslich vom Programm geschrieben. Externe "
            "Aenderungen werden erkannt und abgefragt.");
        ImGui::PopStyleColor();
        if (ImGui::Button("Schliessen", ImVec2(120, 0))) {
            ed.dialogs.about = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    openModal("Tastenkuerzel", ed.dialogs.shortcuts);
    if (ImGui::BeginPopupModal("Tastenkuerzel", &ed.dialogs.shortcuts,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        struct Row {
            const char* keys;
            const char* what;
        };
        Row rows[] = {
            {"Strg+N", "Neues Element"},
            {"Strg+Shift+N", "Neues Projekt"},
            {"Strg+G", "Neue Gruppe"},
            {"Strg+T", "Neue Aktion"},
            {"Strg+S", "Alles speichern"},
            {"Strg+Z / Strg+Y", "Rueckgaengig / Wiederholen"},
            {"Strg+Mausrad", "Timeline zoomen"},
            {"Shift+Mausrad", "Timeline horizontal scrollen"},
            {"Doppelklick Timeline", "Aktion anlegen / bearbeiten"},
            {"Ziehen in Timeline", "Aktion zeitlich verschieben"},
        };
        if (ImGui::BeginTable("keys", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            for (const Row& r : rows) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(r.keys);
                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(r.what);
            }
            ImGui::EndTable();
        }
        if (ImGui::Button("Schliessen", ImVec2(120, 0))) {
            ed.dialogs.shortcuts = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

}  // namespace

// --------------------------------------------------------------- requests
void openNewProject(Editor& ed) {
    ed.dialogs.newProject.open = true;
    if (ed.dialogs.newProject.folder.empty()) ed.dialogs.newProject.folder.clear();
}

void openNewGroup(Editor& ed, const std::string& parentId) {
    GroupDialog& st = ed.dialogs.group;
    st = GroupDialog{};
    st.open = true;
    st.isNew = true;
    st.parentId = parentId;
    st.color = ed.project.autoColorForGroup(parentId);
}

void openEditGroup(Editor& ed, const std::string& groupId) {
    const Group* g = ed.project.group(groupId);
    if (!g) return;
    GroupDialog& st = ed.dialogs.group;
    st = GroupDialog{};
    st.open = true;
    st.isNew = false;
    st.groupId = groupId;
    st.parentId = g->parentId;
    st.name = g->name;
    st.color = ed.project.groupColor(groupId);
    st.explicitColor = g->colorExplicit;
}

void openNewElement(Editor& ed, const std::string& groupId) {
    if (!ed.project.group(groupId)) {
        ed.setStatus("Bitte zuerst eine Gruppe waehlen.", true);
        return;
    }
    ElementDialog& st = ed.dialogs.element;
    st = ElementDialog{};
    st.open = true;
    st.isNew = true;
    st.groupId = groupId;
    for (const FieldDef& f : ed.project.effectiveFields(groupId)) st.values[f.name] = defaultValueFor(f);
}

void openEditElement(Editor& ed, const std::string& elementId) {
    const Element* el = ed.project.element(elementId);
    if (!el) return;
    ElementDialog& st = ed.dialogs.element;
    st = ElementDialog{};
    st.open = true;
    st.isNew = false;
    st.elementId = elementId;
    st.groupId = el->groupId;
    st.name = el->name;
    st.values = el->values;
}

void openTemplate(Editor& ed, const std::string& groupId) {
    const Group* g = ed.project.group(groupId);
    if (!g) return;
    TemplateDialog& st = ed.dialogs.templateEditor;
    st.open = true;
    st.groupId = groupId;
    st.fields = g->fields;
}

void openNewAction(Editor& ed, long long time, const std::string& elementId) {
    ActionDialog& st = ed.dialogs.action;
    st = ActionDialog{};
    st.open = true;
    st.isNew = true;
    st.draft = Action{};
    st.draft.type = ed.project.actionTypes.empty() ? "Action" : ed.project.actionTypes.front();
    st.draft.time = time;
    if (!elementId.empty()) st.draft.elementIds.push_back(elementId);
    st.timeText = formatStoryTime(time);
}

void openEditAction(Editor& ed, const std::string& actionId) {
    const Action* a = ed.project.action(actionId);
    if (!a) return;
    ActionDialog& st = ed.dialogs.action;
    st = ActionDialog{};
    st.open = true;
    st.isNew = false;
    st.draft = *a;
    st.timeText = formatStoryTime(ed.project.resolveActionTime(*a));
    st.tagsText = joinList(a->tags);
}

void openNewConnection(Editor& ed, const std::string& src, const std::string& dst) {
    ConnectionDialog& st = ed.dialogs.connection;
    st = ConnectionDialog{};
    st.open = true;
    st.isNew = true;
    st.draft = Connection{};
    st.draft.sourceId = src;
    st.draft.targetId = dst;
    if (!ed.project.connectionTypes.empty()) st.draft.type = ed.project.connectionTypes.front();
}

void openEditConnection(Editor& ed, const std::string& connectionId) {
    const Connection* conn = ed.project.connection(connectionId);
    if (!conn) return;
    ConnectionDialog& st = ed.dialogs.connection;
    st = ConnectionDialog{};
    st.open = true;
    st.isNew = false;
    st.draft = *conn;
}

void openNewBlock(Editor& ed, const std::vector<std::string>& connectionIds) {
    BlockDialog& st = ed.dialogs.block;
    st = BlockDialog{};
    st.open = true;
    st.isNew = true;
    st.connectionIds = connectionIds;
    st.draft.name = "Block";
}

void openMoveElement(Editor& ed, const std::string& elementId) {
    MoveDialog& st = ed.dialogs.move;
    st = MoveDialog{};
    st.open = true;
    st.elementId = elementId;
    if (const Element* el = ed.project.element(elementId)) st.targetGroupId = el->groupId;
}

void confirm(Editor& ed, const std::string& title, const std::string& message,
             const std::string& details, std::function<void()> onConfirm) {
    ConfirmDialog& st = ed.dialogs.confirm;
    st.open = true;
    st.title = title;
    st.message = message;
    st.details = details;
    st.onConfirm = std::move(onConfirm);
}

void prompt(Editor& ed, const std::string& title, const std::string& label, const std::string& value,
            std::function<void(const std::string&)> onAccept) {
    TextPromptDialog& st = ed.dialogs.prompt;
    st.open = true;
    st.title = title;
    st.label = label;
    st.value = value;
    st.onAccept = std::move(onAccept);
}

void draw(Editor& ed) {
    drawNewProject(ed);
    drawGroupDialog(ed);
    drawElementDialog(ed);
    drawTemplateDialog(ed);
    drawActionDialog(ed);
    drawConnectionDialog(ed);
    drawBlockDialog(ed);
    drawMoveDialog(ed);
    drawConfirmDialog(ed);
    drawPromptDialog(ed);
    drawConflictDialog(ed);
    drawAbout(ed);
}

}  // namespace se::dialogs
