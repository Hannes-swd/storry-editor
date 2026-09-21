// The in-memory project: groups, elements, actions, connections plus the
// queries the windows need on top of them.
#pragma once

#include <deque>
#include <map>
#include <string>
#include <vector>

#include "core/Model.h"

namespace se {

struct Project {
    std::string name;
    std::string vaultPath;  // absolute path of the obsidian vault

    // deques: references handed out by add*() stay valid when more items are
    // appended, which the dialogs and the loader rely on.
    std::deque<Group> groups;
    std::deque<Element> elements;
    std::deque<Action> actions;
    std::deque<Connection> connections;
    std::deque<Block> blocks;

    std::map<std::string, ImVec2> nodePositions;  // connections graph layout
    std::vector<std::string> actionTypes{"Dialogue", "Action", "Scene Change", "Discovery", "Conflict"};
    std::vector<ConnectionType> connectionTypes;
    std::vector<std::string> storylines{"Main Quest"};

    bool loaded = false;

    // ----------------------------------------------------------- lookups
    Group* group(const std::string& id);
    const Group* group(const std::string& id) const;
    Element* element(const std::string& id);
    const Element* element(const std::string& id) const;
    Action* action(const std::string& id);
    const Action* action(const std::string& id) const;
    Connection* connection(const std::string& id);
    Block* block(const std::string& id);
    ConnectionType* connectionType(const std::string& id);
    const ConnectionType* connectionType(const std::string& id) const;
    ConnectionType* connectionTypeByName(const std::string& typeName);
    // Legt bei Bedarf einen einfachen Typ mit zwei Rollen an (Altbestand, Import).
    ConnectionType& ensureConnectionType(const std::string& typeName);
    std::string connectionTypeName(const Connection& c) const;

    Group* findGroupByPath(const std::string& path);
    Element* findElementByPath(const std::string& path);

    std::vector<Group*> childGroups(const std::string& parentId);
    std::vector<const Group*> childGroups(const std::string& parentId) const;
    std::vector<Element*> groupElements(const std::string& groupId);
    std::vector<const Element*> groupElements(const std::string& groupId) const;

    std::string groupPath(const std::string& groupId) const;      // "Characters/Main"
    std::string elementPath(const std::string& elementId) const;  // "Characters/Main/Alice"
    std::string displayName(const std::string& anyId) const;      // element or group name
    std::string rootGroupOf(const std::string& groupId) const;
    bool isAncestorGroup(const std::string& ancestor, const std::string& groupId) const;

    // Template fields of a group: inherited from all parents plus own fields.
    std::vector<FieldDef> effectiveFields(const std::string& groupId) const;
    // Template-Felder der Gruppe plus die elementeigenen Felder.
    std::vector<FieldDef> fieldsForElement(const Element& el) const;
    bool isOwnField(const Element& el, const std::string& fieldName) const;
    void addOwnField(Element& el, const FieldDef& field);
    void removeOwnField(Element& el, const std::string& fieldName);
    const FieldDef* findField(const std::string& groupId, const std::string& fieldName) const;

    ImVec4 groupColor(const std::string& groupId) const;
    ImVec4 elementColor(const std::string& elementId) const;
    ImVec4 colorForId(const std::string& anyId) const;  // element or group
    // Farbe eines Verbindungstyps: eigene Farbe oder automatisch vergeben.
    ImVec4 connectionTypeColor(const std::string& typeId) const;

    // ----------------------------------------------------------- queries
    std::vector<const Action*> sortedActions() const;
    std::vector<const Action*> actionsForElement(const std::string& elementId) const;
    std::vector<const Connection*> connectionsForElement(const std::string& elementId) const;
    // Gilt die Verbindung zu diesem Zeitpunkt? Nicht-zeitliche gelten immer.
    bool connectionActiveAt(const Connection& c, long long time) const;
    // Abschnitte fuer das Timeline-Band: ab wann zeigt das Element worauf.
    struct BandSegment {
        long long start = 0;
        bool hasEnd = false;
        long long end = 0;
        std::string labelElementId;
        std::string connectionId;
    };
    std::vector<BandSegment> bandSegments(const std::string& elementId,
                                          const std::string& typeId) const;
    // Beendet bei exklusiven Typen die vorherige Setzung an derselben Rolle.
    void applyExclusivity(const Connection& newer);
    bool roleAccepts(const ConnectionType& type, size_t role, const std::string& elementId) const;
    // Value of a field at a point in time, mutations applied (spec 3.5.6).
    std::string valueAt(const std::string& elementId, const std::string& field, long long time) const;
    // Every reference to an element: actions, connections and reference fields.
    std::vector<std::string> referencesTo(const std::string& elementId) const;
    long long resolveActionTime(const Action& a) const;

    // ----------------------------------------------------------- mutations
    Group& addGroup(const std::string& groupName, const std::string& parentId);
    Element& addElement(const std::string& elementName, const std::string& groupId);
    Action& addAction(const std::string& title, long long time);
    Connection& addConnection(const std::string& typeId, const std::vector<std::string>& members);
    ConnectionType& addConnectionType(const std::string& typeName);
    void removeConnectionType(const std::string& id);
    Block& addBlock(const std::string& blockName);

    void removeGroup(const std::string& id);  // recursive
    void removeElement(const std::string& id);
    void removeAction(const std::string& id);
    void removeConnection(const std::string& id);
    void removeBlock(const std::string& id);

    // Adds missing template fields to every element of a group (spec 3.2.4).
    void applyTemplateToElements(const std::string& groupId);
    void syncElementFields(Element& el);
    ImVec4 autoColorForGroup(const std::string& parentId) const;
    void clear();
};

}  // namespace se
