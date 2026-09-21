#include "core/Project.h"

#include <algorithm>
#include <cmath>

namespace se {
namespace {

template <typename Container>
typename Container::value_type* findById(Container& v, const std::string& id) {
    for (auto& item : v) {
        if (item.id == id) return &item;
    }
    return nullptr;
}

template <typename Container>
const typename Container::value_type* findById(const Container& v, const std::string& id) {
    for (auto& item : v) {
        if (item.id == id) return &item;
    }
    return nullptr;
}

ImVec4 hsv(float h, float s, float v, float a = 1.0f) {
    float r = 0, g = 0, b = 0;
    ImGui::ColorConvertHSVtoRGB(h, s, v, r, g, b);
    return ImVec4(r, g, b, a);
}

}  // namespace

// ------------------------------------------------------------------ lookups
Group* Project::group(const std::string& id) { return findById(groups, id); }
const Group* Project::group(const std::string& id) const { return findById(groups, id); }
Element* Project::element(const std::string& id) { return findById(elements, id); }
const Element* Project::element(const std::string& id) const { return findById(elements, id); }
Action* Project::action(const std::string& id) { return findById(actions, id); }
const Action* Project::action(const std::string& id) const { return findById(actions, id); }
Connection* Project::connection(const std::string& id) { return findById(connections, id); }
Block* Project::block(const std::string& id) { return findById(blocks, id); }

Group* Project::findGroupByPath(const std::string& path) {
    for (auto& g : groups) {
        if (groupPath(g.id) == path) return &g;
    }
    return nullptr;
}

Element* Project::findElementByPath(const std::string& path) {
    for (auto& e : elements) {
        if (elementPath(e.id) == path) return &e;
    }
    return nullptr;
}

std::vector<Group*> Project::childGroups(const std::string& parentId) {
    std::vector<Group*> out;
    for (auto& g : groups) {
        if (g.parentId == parentId) out.push_back(&g);
    }
    return out;
}

std::vector<const Group*> Project::childGroups(const std::string& parentId) const {
    std::vector<const Group*> out;
    for (auto& g : groups) {
        if (g.parentId == parentId) out.push_back(&g);
    }
    return out;
}

std::vector<Element*> Project::groupElements(const std::string& groupId) {
    std::vector<Element*> out;
    for (auto& e : elements) {
        if (e.groupId == groupId) out.push_back(&e);
    }
    return out;
}

std::vector<const Element*> Project::groupElements(const std::string& groupId) const {
    std::vector<const Element*> out;
    for (auto& e : elements) {
        if (e.groupId == groupId) out.push_back(&e);
    }
    return out;
}

std::string Project::groupPath(const std::string& groupId) const {
    const Group* g = group(groupId);
    if (!g) return std::string();
    std::string path = g->name;
    std::string parent = g->parentId;
    int guard = 0;
    while (!parent.empty() && guard++ < 64) {
        const Group* p = group(parent);
        if (!p) break;
        path = p->name + "/" + path;
        parent = p->parentId;
    }
    return path;
}

std::string Project::elementPath(const std::string& elementId) const {
    const Element* e = element(elementId);
    if (!e) return std::string();
    std::string gp = groupPath(e->groupId);
    return gp.empty() ? e->name : gp + "/" + e->name;
}

std::string Project::displayName(const std::string& anyId) const {
    if (const Element* e = element(anyId)) return e->name;
    if (const Group* g = group(anyId)) return g->name;
    return anyId;
}

std::string Project::rootGroupOf(const std::string& groupId) const {
    const Group* g = group(groupId);
    if (!g) return std::string();
    int guard = 0;
    while (!g->parentId.empty() && guard++ < 64) {
        const Group* p = group(g->parentId);
        if (!p) break;
        g = p;
    }
    return g->id;
}

bool Project::isAncestorGroup(const std::string& ancestor, const std::string& groupId) const {
    const Group* g = group(groupId);
    int guard = 0;
    while (g && guard++ < 64) {
        if (g->id == ancestor) return true;
        g = group(g->parentId);
    }
    return false;
}

std::vector<FieldDef> Project::effectiveFields(const std::string& groupId) const {
    std::vector<const Group*> chain;
    const Group* g = group(groupId);
    int guard = 0;
    while (g && guard++ < 64) {
        chain.push_back(g);
        g = group(g->parentId);
    }
    std::reverse(chain.begin(), chain.end());

    std::vector<FieldDef> out;
    for (const Group* node : chain) {
        for (const FieldDef& f : node->fields) {
            auto it = std::find_if(out.begin(), out.end(),
                                   [&](const FieldDef& e) { return e.name == f.name; });
            if (it == out.end())
                out.push_back(f);
            else
                *it = f;  // the closer group overrides the inherited definition
        }
    }
    return out;
}

const FieldDef* Project::findField(const std::string& groupId, const std::string& fieldName) const {
    static FieldDef cached;
    std::vector<FieldDef> fields = effectiveFields(groupId);
    for (const FieldDef& f : fields) {
        if (f.name == fieldName) {
            cached = f;
            return &cached;
        }
    }
    return nullptr;
}

// ------------------------------------------------------------------- colors
ImVec4 Project::groupColor(const std::string& groupId) const {
    const Group* g = group(groupId);
    if (!g) return ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
    if (g->colorExplicit) return g->color;

    if (g->parentId.empty()) {
        int index = 0;
        for (const auto& other : groups) {
            if (other.parentId.empty()) {
                if (other.id == g->id) break;
                ++index;
            }
        }
        float h = std::fmod(0.11f + 0.618033f * static_cast<float>(index), 1.0f);
        return hsv(h, 0.62f, 0.82f);
    }

    // sub groups keep the parent hue but shift value/saturation so that they
    // stay recognisable as related
    ImVec4 base = groupColor(g->parentId);
    float h = 0, s = 0, v = 0;
    ImGui::ColorConvertRGBtoHSV(base.x, base.y, base.z, h, s, v);
    int siblingIndex = 0;
    for (const auto& other : groups) {
        if (other.parentId == g->parentId) {
            if (other.id == g->id) break;
            ++siblingIndex;
        }
    }
    h = std::fmod(h + 0.035f * static_cast<float>(siblingIndex + 1) + 1.0f, 1.0f);
    v = std::min(0.98f, v * (siblingIndex % 2 == 0 ? 0.82f : 1.12f));
    s = std::min(0.95f, s * 1.05f);
    return hsv(h, s, v);
}

ImVec4 Project::elementColor(const std::string& elementId) const {
    const Element* e = element(elementId);
    if (!e) return ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
    return groupColor(e->groupId);
}

ImVec4 Project::colorForId(const std::string& anyId) const {
    if (element(anyId)) return elementColor(anyId);
    if (group(anyId)) return groupColor(anyId);
    return ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
}

ImVec4 Project::autoColorForGroup(const std::string& parentId) const {
    if (parentId.empty()) {
        int count = 0;
        for (const auto& g : groups) {
            if (g.parentId.empty()) ++count;
        }
        float h = std::fmod(0.11f + 0.618033f * static_cast<float>(count), 1.0f);
        return hsv(h, 0.62f, 0.82f);
    }
    ImVec4 base = groupColor(parentId);
    float h = 0, s = 0, v = 0;
    ImGui::ColorConvertRGBtoHSV(base.x, base.y, base.z, h, s, v);
    int count = 0;
    for (const auto& g : groups) {
        if (g.parentId == parentId) ++count;
    }
    h = std::fmod(h + 0.035f * static_cast<float>(count + 1) + 1.0f, 1.0f);
    v = std::min(0.98f, v * (count % 2 == 0 ? 0.82f : 1.12f));
    return hsv(h, std::min(0.95f, s * 1.05f), v);
}

// ------------------------------------------------------------------ queries
long long Project::resolveActionTime(const Action& a) const {
    if (!a.useRelative || a.relativeToId.empty()) return a.time;
    const Action* ref = action(a.relativeToId);
    if (!ref || ref->id == a.id) return a.time;
    // one level of indirection is resolved recursively, cycles fall back to the
    // stored absolute time
    static thread_local int depth = 0;
    if (depth > 16) return a.time;
    ++depth;
    long long base = resolveActionTime(*ref);
    --depth;
    return base + a.relativeOffset;
}

std::vector<const Action*> Project::sortedActions() const {
    std::vector<const Action*> out;
    out.reserve(actions.size());
    for (const auto& a : actions) out.push_back(&a);
    std::sort(out.begin(), out.end(), [this](const Action* l, const Action* r) {
        long long lt = resolveActionTime(*l), rt = resolveActionTime(*r);
        if (lt != rt) return lt < rt;
        return l->title < r->title;
    });
    return out;
}

std::vector<const Action*> Project::actionsForElement(const std::string& elementId) const {
    std::vector<const Action*> out;
    for (const Action* a : sortedActions()) {
        if (std::find(a->elementIds.begin(), a->elementIds.end(), elementId) != a->elementIds.end()) {
            out.push_back(a);
            continue;
        }
        for (const Mutation& m : a->mutations) {
            if (m.elementId == elementId) {
                out.push_back(a);
                break;
            }
        }
    }
    return out;
}

std::vector<const Connection*> Project::connectionsForElement(const std::string& elementId) const {
    std::vector<const Connection*> out;
    for (const auto& c : connections) {
        if (c.sourceId == elementId || c.targetId == elementId) out.push_back(&c);
    }
    return out;
}

std::string Project::valueAt(const std::string& elementId, const std::string& field,
                             long long time) const {
    const Element* e = element(elementId);
    if (!e) return std::string();
    auto it = e->values.find(field);
    std::string value = it != e->values.end() ? it->second : std::string();

    for (const Action* a : sortedActions()) {
        if (resolveActionTime(*a) > time) break;
        for (const Mutation& m : a->mutations) {
            if (m.elementId == elementId && m.field == field) value = m.newValue;
        }
    }
    return value;
}

std::vector<std::string> Project::referencesTo(const std::string& elementId) const {
    std::vector<std::string> out;
    const std::string path = elementPath(elementId);
    for (const auto& a : actions) {
        bool hit = std::find(a.elementIds.begin(), a.elementIds.end(), elementId) != a.elementIds.end();
        for (const Mutation& m : a.mutations) {
            if (m.elementId == elementId) hit = true;
        }
        if (hit) out.push_back("Aktion: " + a.title);
    }
    for (const auto& c : connections) {
        if (c.sourceId == elementId || c.targetId == elementId) {
            const std::string other = c.sourceId == elementId ? c.targetId : c.sourceId;
            out.push_back("Connection: " + c.type + " -> " + displayName(other));
        }
    }
    for (const auto& e : elements) {
        if (e.id == elementId) continue;
        for (const auto& kv : e.values) {
            if (!kv.second.empty() && (kv.second == "@" + path || kv.second == path)) {
                out.push_back("Feld: " + e.name + "." + kv.first);
            }
        }
    }
    return out;
}

// ---------------------------------------------------------------- mutations
Group& Project::addGroup(const std::string& groupName, const std::string& parentId) {
    Group g;
    g.id = newId("grp");
    g.name = groupName;
    g.parentId = parentId;
    g.color = autoColorForGroup(parentId);
    g.colorExplicit = false;
    groups.push_back(g);
    return groups.back();
}

Element& Project::addElement(const std::string& elementName, const std::string& groupId) {
    Element e;
    e.id = newId("el");
    e.name = elementName;
    e.groupId = groupId;
    elements.push_back(e);
    syncElementFields(elements.back());
    return elements.back();
}

Action& Project::addAction(const std::string& title, long long time) {
    Action a;
    a.id = newId("act");
    a.title = title;
    a.time = time;
    actions.push_back(a);
    return actions.back();
}

Connection& Project::addConnection(const std::string& src, const std::string& dst,
                                   const std::string& type) {
    Connection c;
    c.id = newId("conn");
    c.sourceId = src;
    c.targetId = dst;
    c.type = type;
    connections.push_back(c);
    return connections.back();
}

Block& Project::addBlock(const std::string& blockName) {
    Block b;
    b.id = newId("blk");
    b.name = blockName;
    blocks.push_back(b);
    return blocks.back();
}

void Project::removeGroup(const std::string& id) {
    for (Group* child : childGroups(id)) {
        std::string childId = child->id;
        removeGroup(childId);
    }
    std::vector<std::string> doomed;
    for (const auto& e : elements) {
        if (e.groupId == id) doomed.push_back(e.id);
    }
    for (const auto& eid : doomed) removeElement(eid);
    groups.erase(std::remove_if(groups.begin(), groups.end(),
                                [&](const Group& g) { return g.id == id; }),
                 groups.end());
}

void Project::removeElement(const std::string& id) {
    for (auto& a : actions) {
        a.elementIds.erase(std::remove(a.elementIds.begin(), a.elementIds.end(), id),
                           a.elementIds.end());
        a.mutations.erase(std::remove_if(a.mutations.begin(), a.mutations.end(),
                                         [&](const Mutation& m) { return m.elementId == id; }),
                          a.mutations.end());
    }
    connections.erase(std::remove_if(connections.begin(), connections.end(),
                                     [&](const Connection& c) {
                                         return c.sourceId == id || c.targetId == id;
                                     }),
                      connections.end());
    nodePositions.erase(id);
    elements.erase(std::remove_if(elements.begin(), elements.end(),
                                  [&](const Element& e) { return e.id == id; }),
                   elements.end());
}

void Project::removeAction(const std::string& id) {
    for (auto& a : actions) {
        if (a.relativeToId == id) {
            a.time = resolveActionTime(a);
            a.useRelative = false;
            a.relativeToId.clear();
        }
    }
    actions.erase(std::remove_if(actions.begin(), actions.end(),
                                 [&](const Action& a) { return a.id == id; }),
                  actions.end());
}

void Project::removeConnection(const std::string& id) {
    connections.erase(std::remove_if(connections.begin(), connections.end(),
                                     [&](const Connection& c) { return c.id == id; }),
                      connections.end());
}

void Project::removeBlock(const std::string& id) {
    for (auto& c : connections) {
        if (c.blockId == id) c.blockId.clear();
    }
    blocks.erase(std::remove_if(blocks.begin(), blocks.end(),
                                [&](const Block& b) { return b.id == id; }),
                 blocks.end());
}

void Project::syncElementFields(Element& el) {
    std::vector<FieldDef> fields = effectiveFields(el.groupId);
    for (const FieldDef& f : fields) {
        if (el.values.find(f.name) == el.values.end()) el.values[f.name] = defaultValueFor(f);
        if (std::find(el.fieldOrder.begin(), el.fieldOrder.end(), f.name) == el.fieldOrder.end())
            el.fieldOrder.push_back(f.name);
    }
    for (const auto& kv : el.values) {
        if (std::find(el.fieldOrder.begin(), el.fieldOrder.end(), kv.first) == el.fieldOrder.end())
            el.fieldOrder.push_back(kv.first);
    }
    // template fields first, user added fields afterwards
    std::stable_sort(el.fieldOrder.begin(), el.fieldOrder.end(),
                     [&](const std::string& a, const std::string& b) {
                         auto rank = [&](const std::string& n) {
                             for (size_t i = 0; i < fields.size(); ++i) {
                                 if (fields[i].name == n) return static_cast<int>(i);
                             }
                             return 1000;
                         };
                         return rank(a) < rank(b);
                     });
}

void Project::applyTemplateToElements(const std::string& groupId) {
    for (auto& e : elements) {
        if (isAncestorGroup(groupId, e.groupId)) syncElementFields(e);
    }
}

void Project::clear() {
    name.clear();
    vaultPath.clear();
    groups.clear();
    elements.clear();
    actions.clear();
    connections.clear();
    blocks.clear();
    nodePositions.clear();
    loaded = false;
}

}  // namespace se
