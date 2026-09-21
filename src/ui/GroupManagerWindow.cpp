// Group manager (spec 3.2) and the shared details panel (spec 3.1.7 / 3.2.3).
#include <algorithm>
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

struct GroupManagerState {
    std::string search;
    bool showElements = true;
};

GroupManagerState& gmState() {
    static GroupManagerState s;
    return s;
}

void duplicateElement(Editor& ed, const std::string& id) {
    const Element* src = ed.project.element(id);
    if (!src) return;
    ed.pushUndo(TR("Element dupliziert"));
    Element copy = *src;
    copy.id = newId("el");
    copy.name = src->name + TR(" (Kopie)");
    copy.filePath.clear();
    ed.project.elements.push_back(copy);
    ed.markElement(copy.id);
    ed.select(SelKind::Element, copy.id);
}

void deleteElementWithWarning(Editor& ed, const std::string& id) {
    const Element* el = ed.project.element(id);
    if (!el) return;
    std::vector<std::string> refs = ed.project.referencesTo(id);
    std::string details;
    if (!refs.empty()) {
        details = TR("Folgende Verweise werden ungueltig:\n");
        for (const std::string& r : refs) details += "  - " + r + "\n";
    }
    std::string name = el->name;
    dialogs::confirm(ed, TR("Element loeschen"), "\"" + name + TR("\" inklusive .md-Datei loeschen?"), details,
                     [&ed, id]() {
                         ed.pushUndo(TR("Element geloescht"));
                         ed.deleteElementWithFile(id);
                     });
}

void elementContextMenu(Editor& ed, const std::string& id) {
    if (!ImGui::BeginPopupContextItem()) return;
    const Element* el = ed.project.element(id);
    if (el) ImGui::TextDisabled("%s", ed.project.elementPath(id).c_str());
    ImGui::Separator();
    if (ImGui::MenuItem(TR("Bearbeiten..."))) dialogs::openEditElement(ed, id);
    if (ImGui::MenuItem(TR("Umbenennen..."))) {
        std::string current = el ? el->name : "";
        dialogs::prompt(ed, TR("Element umbenennen"), TR("Neuer Name"), current,
                        [&ed, id](const std::string& value) {
                            ed.pushUndo(TR("Element umbenannt"));
                            ed.renameElement(id, value);
                        });
    }
    if (ImGui::MenuItem(TR("Duplizieren"))) duplicateElement(ed, id);
    if (ImGui::MenuItem(TR("Verschieben nach..."))) dialogs::openMoveElement(ed, id);
    if (ImGui::MenuItem(TR("Als Aktion in Timeline eintragen"))) dialogs::openNewAction(ed, 0, id);
    if (ImGui::MenuItem(TR("Datei im Explorer zeigen")) && el)
        platform::openInShell(vault::absolutePath(ed.project, el->filePath));
    ImGui::Separator();
    if (ImGui::MenuItem(TR("Loeschen"))) deleteElementWithWarning(ed, id);
    ImGui::EndPopup();
}

void groupContextMenu(Editor& ed, const std::string& id) {
    if (!ImGui::BeginPopupContextItem()) return;
    ImGui::TextDisabled("%s", ed.project.groupPath(id).c_str());
    ImGui::Separator();
    if (ImGui::MenuItem(TR("Neues Element..."))) dialogs::openNewElement(ed, id);
    if (ImGui::MenuItem(TR("Neue Untergruppe..."))) dialogs::openNewGroup(ed, id);
    if (ImGui::MenuItem(TR("Template bearbeiten..."))) dialogs::openTemplate(ed, id);
    if (ImGui::MenuItem(TR("Gruppe bearbeiten (Name/Farbe)..."))) dialogs::openEditGroup(ed, id);
    ImGui::Separator();
    if (ImGui::MenuItem(TR("Loeschen"))) {
        const Group* g = ed.project.group(id);
        std::string name = g ? g->name : "";
        int elementCount = 0;
        for (const Element& e : ed.project.elements) {
            if (ed.project.isAncestorGroup(id, e.groupId)) ++elementCount;
        }
        std::string details = elementCount > 0
                                  ? std::to_string(elementCount) +
                                        TR(" Element(e) inkl. Untergruppen werden mitgeloescht.")
                                  : std::string();
        dialogs::confirm(ed, TR("Gruppe loeschen"), TR("Gruppe \"") + name + TR("\" loeschen?"), details,
                         [&ed, id]() {
                             ed.pushUndo(TR("Gruppe geloescht"));
                             std::vector<std::string> doomed;
                             for (const Element& e : ed.project.elements) {
                                 if (ed.project.isAncestorGroup(id, e.groupId)) doomed.push_back(e.id);
                             }
                             for (const std::string& eid : doomed) ed.deleteElementWithFile(eid);
                             ed.project.removeGroup(id);
                             ed.markMetadata();
                             ed.markAllElements();
                         });
    }
    ImGui::EndPopup();
}

void drawElementNode(Editor& ed, Element& el) {
    GroupManagerState& st = gmState();
    if (!st.search.empty() && !iequalsContains(el.name, st.search)) return;

    ImGui::PushID(el.id.c_str());
    ui::colorDot(ed.project.elementColor(el.id));
    bool selected = ed.selection.is(SelKind::Element, el.id);
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen |
                               ImGuiTreeNodeFlags_SpanAvailWidth;
    if (selected) flags |= ImGuiTreeNodeFlags_Selected;
    ImGui::TreeNodeEx("##node", flags, "%s", el.name.c_str());

    if (ed.focusElementId == el.id) {
        ImGui::SetScrollHereY(0.5f);
        ed.focusElementId.clear();
    }
    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) ed.select(SelKind::Element, el.id);
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        dialogs::openEditElement(ed, el.id);

    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
        const char* idStr = el.id.c_str();
        ImGui::SetDragDropPayload("SE_ELEMENT", idStr, el.id.size() + 1);
        ImGui::TextUnformatted(el.name.c_str());
        ImGui::EndDragDropSource();
    }
    elementContextMenu(ed, el.id);
    ImGui::PopID();
}

void drawGroupNode(Editor& ed, Group& g) {
    ImGui::PushID(g.id.c_str());
    ui::colorDot(ed.project.groupColor(g.id));

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth |
                               ImGuiTreeNodeFlags_OpenOnDoubleClick;
    if (ed.selection.is(SelKind::Group, g.id)) flags |= ImGuiTreeNodeFlags_Selected;
    ImGui::SetNextItemOpen(g.expanded, ImGuiCond_Always);
    bool open = ImGui::TreeNodeEx("##group", flags, "%s", g.name.c_str());
    if (open != g.expanded) {
        g.expanded = open;
        ed.markMetadata();
    }
    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) ed.select(SelKind::Group, g.id);

    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SE_ELEMENT")) {
            std::string elementId(static_cast<const char*>(payload->Data));
            ed.pushUndo(TR("Element verschoben"));
            ed.moveElement(elementId, g.id);
            ed.setStatus(TR("Element nach ") + ed.project.groupPath(g.id) + TR(" verschoben"));
        }
        ImGui::EndDragDropTarget();
    }
    groupContextMenu(ed, g.id);

    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, theme::colors().textSecondary);
    int count = static_cast<int>(ed.project.groupElements(g.id).size());
    ImGui::Text("(%d)", count);
    ImGui::PopStyleColor();

    if (open) {
        for (Group* child : ed.project.childGroups(g.id)) drawGroupNode(ed, *child);
        if (gmState().showElements) {
            for (Element* el : ed.project.groupElements(g.id)) drawElementNode(ed, *el);
        }
        ImGui::TreePop();
    }
    ImGui::PopID();
}

// --------------------------------------------------------------- details
void drawElementDetails(Editor& ed, Element& el) {
    ColorScheme& c = theme::colors();
    ui::colorDot(ed.project.elementColor(el.id), 7.0f);
    ImGui::PushFont(nullptr, ImGui::GetFontSize() * 1.25f);
    ImGui::TextUnformatted(el.name.c_str());
    ImGui::PopFont();
    ui::textSecondary(ed.project.elementPath(el.id).c_str());
    ImGui::Separator();

    if (ImGui::Button(TR("Bearbeiten"))) dialogs::openEditElement(ed, el.id);
    ImGui::SameLine();
    if (ImGui::Button(TR("Duplizieren"))) duplicateElement(ed, el.id);
    ImGui::SameLine();
    if (ImGui::Button(TR("Aktion hinzufuegen"))) dialogs::openNewAction(ed, 0, el.id);
    ImGui::SameLine();
    if (ImGui::Button(TR("Datei oeffnen")))
        platform::openInShell(vault::absolutePath(ed.project, el.filePath));
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, theme::withAlpha(c.errorColor, 0.7f));
    if (ImGui::Button(TR("Loeschen"))) deleteElementWithWarning(ed, el.id);
    ImGui::PopStyleColor();

    if (ImGui::CollapsingHeader(TR("Felder"), ImGuiTreeNodeFlags_DefaultOpen)) {
        std::vector<FieldDef> fields = ed.project.effectiveFields(el.groupId);
        for (const std::string& key : el.fieldOrder) {
            auto vit = el.values.find(key);
            if (vit == el.values.end()) continue;
            const FieldDef* def = nullptr;
            for (const FieldDef& f : fields) {
                if (f.name == key) def = &f;
            }
            FieldDef fallback;
            fallback.name = key;
            if (!def) def = &fallback;

            ImGui::PushID(key.c_str());
            ImGui::TextUnformatted(key.c_str());
            if (def->required) {
                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_Text, c.warningColor);
                ImGui::TextUnformatted("*");
                ImGui::PopStyleColor();
            }
            std::string value = vit->second;
            if (ui::fieldValueEditor(ed, *def, value, key.c_str(), el.groupId)) {
                ed.pushUndo(TR("Feld geaendert"));
                el.values[key] = value;
                if (key == "name" && !value.empty()) ed.renameElement(el.id, value);
                ed.markElement(el.id);
            }
            // show how the value changes over time
            std::vector<std::string> changes;
            for (const Action* a : ed.project.sortedActions()) {
                for (const Mutation& m : a->mutations) {
                    if (m.elementId == el.id && m.field == key) {
                        changes.push_back(formatStoryTime(ed.project.resolveActionTime(*a)) + ": " +
                                          m.oldValue + " -> " + m.newValue);
                    }
                }
            }
            if (!changes.empty()) {
                ImGui::PushStyleColor(ImGuiCol_Text, c.textSecondary);
                for (const std::string& ch : changes) ImGui::BulletText("%s", ch.c_str());
                ImGui::PopStyleColor();
            }
            ImGui::PopID();
            ImGui::Spacing();
        }
        if (ImGui::SmallButton(TR("+ Eigenes Feld"))) {
            dialogs::prompt(ed, TR("Neues Feld"), TR("Feldname"), "", [&ed, id = el.id](const std::string& v) {
                Element* target = ed.project.element(id);
                if (!target || v.empty()) return;
                ed.pushUndo(TR("Feld hinzugefuegt"));
                target->values[v] = "";
                target->fieldOrder.push_back(v);
                ed.markElement(id);
            });
        }
    }

    if (ImGui::CollapsingHeader(TR("Freitext"))) {
        std::string body = el.body;
        if (ImGui::InputTextMultiline("##body", &body, ImVec2(-1, 140))) {
            el.body = body;
            ed.markElement(el.id);
        }
    }

    if (ImGui::CollapsingHeader(TR("Beziehungen"), ImGuiTreeNodeFlags_DefaultOpen)) {
        std::vector<const Connection*> conns = ed.project.connectionsForElement(el.id);
        if (conns.empty()) ui::textSecondary(TR("Keine Verbindungen."));
        for (const Connection* conn : conns) {
            std::string other = conn->sourceId == el.id ? conn->targetId : conn->sourceId;
            ImGui::TextUnformatted(conn->type.c_str());
            ui::elementChip(ed, other);
            ImGui::SameLine();
            ImGui::PushID(conn->id.c_str());
            if (ImGui::SmallButton(TR("Bearbeiten"))) dialogs::openEditConnection(ed, conn->id);
            ImGui::PopID();
        }
        if (ImGui::SmallButton(TR("+ Verbindung"))) dialogs::openNewConnection(ed, el.id, "");
    }

    if (ImGui::CollapsingHeader(TR("Erwaehnt in Aktionen"), ImGuiTreeNodeFlags_DefaultOpen)) {
        std::vector<const Action*> acts = ed.project.actionsForElement(el.id);
        ImGui::Text("%d Aktion(en)", static_cast<int>(acts.size()));
        for (const Action* a : acts) {
            ImGui::PushID(a->id.c_str());
            ui::textSecondary(formatStoryTime(ed.project.resolveActionTime(*a)).c_str());
            ImGui::SameLine();
            if (ImGui::SmallButton(a->title.c_str())) {
                ed.select(SelKind::Action, a->id);
                ed.focusActionId = a->id;
                ed.focusTimeline = true;
            }
            ImGui::PopID();
        }
    }
}

void drawActionDetails(Editor& ed, Action& a) {
    ColorScheme& c = theme::colors();
    ImGui::PushFont(nullptr, ImGui::GetFontSize() * 1.25f);
    ImGui::TextUnformatted(a.title.c_str());
    ImGui::PopFont();
    long long t = ed.project.resolveActionTime(a);
    ui::textSecondary(formatStoryTimeLong(t).c_str());
    if (a.useRelative && !a.relativeToId.empty()) {
        const Action* ref = ed.project.action(a.relativeToId);
        if (ref)
            ui::textSecondary((formatDuration(a.relativeOffset) + " nach \"" + ref->title + "\"").c_str());
    }
    ImGui::Separator();

    if (ImGui::Button(TR("Bearbeiten"))) dialogs::openEditAction(ed, a.id);
    ImGui::SameLine();
    if (ImGui::Button(TR("In Timeline zeigen"))) {
        ed.focusActionId = a.id;
        ed.focusTimeline = true;
        theme::settings().showTimeline = true;
    }
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, theme::withAlpha(c.errorColor, 0.7f));
    if (ImGui::Button(TR("Loeschen"))) {
        std::string id = a.id;
        dialogs::confirm(ed, TR("Aktion loeschen"), TR("Aktion \"") + a.title + TR("\" loeschen?"), "",
                         [&ed, id]() {
                             ed.pushUndo(TR("Aktion geloescht"));
                             ed.project.removeAction(id);
                             ed.markActions();
                         });
    }
    ImGui::PopStyleColor();

    ImGui::TextWrapped("%s", a.description.c_str());
    ImGui::Spacing();
    ui::textSecondary((TR("Typ: ") + a.type + (a.major ? TR("  |  wichtig") : "")).c_str());
    if (!a.storyline.empty()) ui::textSecondary((TR("Handlungsstrang: ") + a.storyline).c_str());

    if (!a.elementIds.empty()) {
        ImGui::TextUnformatted(TR("Beteiligte:"));
        for (const std::string& id : a.elementIds) ui::elementChip(ed, id);
        ImGui::NewLine();
    }
    if (!a.tags.empty()) ui::textSecondary((TR("Tags: ") + joinList(a.tags)).c_str());

    if (!a.mutations.empty()) {
        ImGui::TextUnformatted(TR("Attribut-Aenderungen:"));
        for (const Mutation& m : a.mutations) {
            ImGui::BulletText("%s.%s: %s -> %s", ed.project.displayName(m.elementId).c_str(),
                              m.field.c_str(), m.oldValue.c_str(), m.newValue.c_str());
        }
    }
    if (!a.attachments.empty()) {
        ImGui::TextUnformatted(TR("Anhaenge:"));
        for (const std::string& att : a.attachments) {
            ImGui::PushID(att.c_str());
            if (ImGui::SmallButton(att.c_str()))
                platform::openInShell(vault::absolutePath(ed.project, att));
            ImGui::PopID();
        }
    }
}

void drawGroupDetails(Editor& ed, Group& g) {
    ui::colorDot(ed.project.groupColor(g.id), 7.0f);
    ImGui::PushFont(nullptr, ImGui::GetFontSize() * 1.25f);
    ImGui::TextUnformatted(g.name.c_str());
    ImGui::PopFont();
    ui::textSecondary(ed.project.groupPath(g.id).c_str());
    ImGui::Separator();

    if (ImGui::Button(TR("Template bearbeiten"))) dialogs::openTemplate(ed, g.id);
    ImGui::SameLine();
    if (ImGui::Button(TR("Neues Element"))) dialogs::openNewElement(ed, g.id);
    ImGui::SameLine();
    if (ImGui::Button(TR("Untergruppe"))) dialogs::openNewGroup(ed, g.id);
    ImGui::SameLine();
    if (ImGui::Button(TR("Bearbeiten"))) dialogs::openEditGroup(ed, g.id);

    ImGui::Spacing();
    ImGui::TextUnformatted(TR("Felder (inkl. geerbt):"));
    for (const FieldDef& f : ed.project.effectiveFields(g.id)) {
        bool own = false;
        for (const FieldDef& o : g.fields) {
            if (o.name == f.name) own = true;
        }
        ImGui::BulletText("%s  [%s]%s%s", f.name.c_str(), fieldTypeName(f.type),
                          f.required ? "  *" : "", own ? "" : TR("  (geerbt)"));
    }
    ImGui::Spacing();
    ImGui::Text(TR("Elemente: %d"), static_cast<int>(ed.project.groupElements(g.id).size()));
}

void drawConnectionDetails(Editor& ed, Connection& conn) {
    ImGui::PushFont(nullptr, ImGui::GetFontSize() * 1.25f);
    ImGui::TextUnformatted(conn.type.c_str());
    ImGui::PopFont();
    ImGui::Separator();
    ui::elementChip(ed, conn.sourceId, false);
    ImGui::SameLine();
    ImGui::TextUnformatted("->");
    ui::elementChip(ed, conn.targetId);
    ImGui::NewLine();
    ImGui::TextWrapped("%s", conn.description.c_str());
    if (!conn.startDate.empty()) ui::textSecondary((TR("Von: ") + conn.startDate).c_str());
    if (!conn.endDate.empty()) ui::textSecondary((TR("Bis: ") + conn.endDate).c_str());
    if (ImGui::Button(TR("Bearbeiten"))) dialogs::openEditConnection(ed, conn.id);
}

}  // namespace

void drawGroupManagerWindow(Editor& ed, bool* open) {
    ImGui::SetNextWindowSize(ImVec2(320, 520), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(TWIN("Gruppen-Manager", "groups"), open)) {
        ImGui::End();
        return;
    }
    if (!ed.project.loaded) {
        ui::textSecondary(TR("Kein Projekt geoeffnet."));
        ImGui::End();
        return;
    }
    GroupManagerState& st = gmState();

    if (ImGui::Button(TR("+ Gruppe"))) dialogs::openNewGroup(ed, "");
    ImGui::SameLine();
    bool canAddElement = ed.selection.kind == SelKind::Group ||
                         (ed.selection.kind == SelKind::Element && ed.project.element(ed.selection.id));
    if (!canAddElement) ImGui::BeginDisabled();
    if (ImGui::Button(TR("+ Element"))) {
        std::string groupId = ed.selection.id;
        if (ed.selection.kind == SelKind::Element) {
            if (const Element* el = ed.project.element(ed.selection.id)) groupId = el->groupId;
        }
        dialogs::openNewElement(ed, groupId);
    }
    if (!canAddElement) ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::Checkbox(TR("Elemente"), &st.showElements);

    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##search", TR("Suchen..."), &st.search);
    ImGui::Separator();

    ImGui::BeginChild("tree", ImVec2(0, 0), ImGuiChildFlags_Borders);
    for (Group* g : ed.project.childGroups("")) drawGroupNode(ed, *g);
    if (ed.project.groups.empty()) ui::textSecondary(TR("Noch keine Gruppen. Lege eine an: + Gruppe"));

    // drop onto empty space = move to top level is not allowed (elements need a group)
    ImGui::EndChild();
    ImGui::End();
}

void drawDetailsWindow(Editor& ed, bool* open) {
    ImGui::SetNextWindowSize(ImVec2(380, 520), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(TWIN("Details", "details"), open)) {
        ImGui::End();
        return;
    }
    if (!ed.project.loaded) {
        ui::textSecondary(TR("Kein Projekt geoeffnet."));
        ImGui::End();
        return;
    }

    switch (ed.selection.kind) {
        case SelKind::Element: {
            if (Element* el = ed.project.element(ed.selection.id))
                drawElementDetails(ed, *el);
            else
                ui::textSecondary(TR("Element nicht gefunden."));
            break;
        }
        case SelKind::Action: {
            if (Action* a = ed.project.action(ed.selection.id))
                drawActionDetails(ed, *a);
            else
                ui::textSecondary(TR("Aktion nicht gefunden."));
            break;
        }
        case SelKind::Group: {
            if (Group* g = ed.project.group(ed.selection.id))
                drawGroupDetails(ed, *g);
            else
                ui::textSecondary(TR("Gruppe nicht gefunden."));
            break;
        }
        case SelKind::Connection: {
            if (Connection* conn = ed.project.connection(ed.selection.id))
                drawConnectionDetails(ed, *conn);
            else
                ui::textSecondary(TR("Verbindung nicht gefunden."));
            break;
        }
        default:
            ui::textSecondary(TR("Nichts ausgewaehlt. Element, Aktion oder Gruppe anklicken."));
            break;
    }
    ImGui::End();
}

}  // namespace se
