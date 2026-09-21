#include "core/VaultIO.h"

#include <algorithm>
#include <filesystem>
#include <sstream>

#include "app/Platform.h"
#include "core/StoryTime.h"

namespace fs = std::filesystem;
using nlohmann::json;

namespace se::vault {
namespace {

const char* managedNote() {
    return englishTexts()
               ? "> This file is managed by Story Editor. Please edit it through the UI only."
               : "> Diese Datei wird vom Story Editor verwaltet. Bitte ausschliesslich ueber die UI "
                 "bearbeiten.";
}

// Abschnittsnamen folgen der UI-Sprache; gelesen werden immer beide.
// Section names follow the UI language; both are always accepted when reading.
const char* sectionFields() { return englishTexts() ? "Fields" : "Felder"; }
const char* sectionText() { return "Text"; }
const char* sectionRelations() { return englishTexts() ? "Relations" : "Beziehungen"; }
const char* sectionActions() { return englishTexts() ? "Linked actions" : "Verknuepfte Aktionen"; }

std::string escapeInline(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (c == '\n')
            out += "\\n";
        else if (c == '\r')
            continue;
        else if (c == '\\')
            out += "\\\\";
        else
            out += c;
    }
    return out;
}

std::string unescapeInline(const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            if (s[i + 1] == 'n') {
                out += '\n';
                ++i;
                continue;
            }
            if (s[i + 1] == '\\') {
                out += '\\';
                ++i;
                continue;
            }
        }
        out += s[i];
    }
    return out;
}

json colorToJson(const ImVec4& c) { return json::array({c.x, c.y, c.z, c.w}); }

ImVec4 colorFromJson(const json& j, const ImVec4& fallback) {
    if (j.is_array() && j.size() >= 3) {
        return ImVec4(j[0].get<float>(), j[1].get<float>(), j[2].get<float>(),
                      j.size() > 3 ? j[3].get<float>() : 1.0f);
    }
    return fallback;
}

json fieldToJson(const FieldDef& f) {
    json j;
    j["name"] = f.name;
    j["type"] = fieldTypeName(f.type);
    j["required"] = f.required;
    j["default"] = f.defaultValue;
    j["description"] = f.description;
    j["display_format"] = f.displayFormat;
    j["enum_options"] = f.enumOptions;
    return j;
}

FieldDef fieldFromJson(const json& j) {
    FieldDef f;
    f.name = j.value("name", "");
    f.type = fieldTypeFromName(j.value("type", "Text"));
    f.required = j.value("required", false);
    f.defaultValue = j.value("default", "");
    f.description = j.value("description", "");
    f.displayFormat = j.value("display_format", "");
    if (j.contains("enum_options") && j["enum_options"].is_array()) {
        for (auto& o : j["enum_options"]) f.enumOptions.push_back(o.get<std::string>());
    }
    return f;
}

json groupToJson(const Group& g) {
    json j;
    j["id"] = g.id;
    j["name"] = g.name;
    j["parent"] = g.parentId;
    j["color"] = colorToJson(g.color);
    j["color_explicit"] = g.colorExplicit;
    j["expanded"] = g.expanded;
    j["fields"] = json::array();
    for (const FieldDef& f : g.fields) j["fields"].push_back(fieldToJson(f));
    return j;
}

Group groupFromJson(const json& j) {
    Group g;
    g.id = j.value("id", newId("grp"));
    g.name = j.value("name", "Gruppe");
    g.parentId = j.value("parent", "");
    g.color = colorFromJson(j.value("color", json()), ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
    g.colorExplicit = j.value("color_explicit", false);
    g.expanded = j.value("expanded", true);
    if (j.contains("fields") && j["fields"].is_array()) {
        for (auto& f : j["fields"]) g.fields.push_back(fieldFromJson(f));
    }
    return g;
}

json actionToJson(const Action& a) {
    json j;
    j["id"] = a.id;
    j["title"] = a.title;
    j["description"] = a.description;
    j["type"] = a.type;
    j["time"] = a.time;
    j["timestamp"] = formatStoryTime(a.time);
    j["use_relative"] = a.useRelative;
    j["relative_to"] = a.relativeToId;
    j["relative_offset"] = a.relativeOffset;
    j["elements"] = a.elementIds;
    j["tags"] = a.tags;
    j["attachments"] = a.attachments;
    j["major"] = a.major;
    j["storyline"] = a.storyline;
    j["mutations"] = json::array();
    for (const Mutation& m : a.mutations) {
        j["mutations"].push_back({{"element", m.elementId},
                                  {"field", m.field},
                                  {"old_value", m.oldValue},
                                  {"new_value", m.newValue}});
    }
    return j;
}

Action actionFromJson(const json& j) {
    Action a;
    a.id = j.value("id", newId("act"));
    a.title = j.value("title", "Aktion");
    a.description = j.value("description", "");
    a.type = j.value("type", "Action");
    a.time = j.value("time", 0LL);
    a.useRelative = j.value("use_relative", false);
    a.relativeToId = j.value("relative_to", "");
    a.relativeOffset = j.value("relative_offset", 0LL);
    a.major = j.value("major", false);
    a.storyline = j.value("storyline", "");
    if (j.contains("elements")) {
        for (auto& e : j["elements"]) a.elementIds.push_back(e.get<std::string>());
    }
    if (j.contains("tags")) {
        for (auto& t : j["tags"]) a.tags.push_back(t.get<std::string>());
    }
    if (j.contains("attachments")) {
        for (auto& t : j["attachments"]) a.attachments.push_back(t.get<std::string>());
    }
    if (j.contains("mutations")) {
        for (auto& m : j["mutations"]) {
            Mutation mu;
            mu.elementId = m.value("element", "");
            mu.field = m.value("field", "");
            mu.oldValue = m.value("old_value", "");
            mu.newValue = m.value("new_value", "");
            a.mutations.push_back(mu);
        }
    }
    return a;
}

json connectionTypeToJson(const ConnectionType& t) {
    json roles = json::array();
    for (const ConnectionRole& r : t.roles) {
        roles.push_back({{"name", r.name}, {"groups", r.allowedGroups}});
    }
    return json{{"id", t.id},
                {"name", t.name},
                {"description", t.description},
                {"roles", roles},
                {"temporal", t.temporal},
                {"exclusive", t.exclusive},
                {"band_role", t.bandRole},
                {"label_role", t.labelRole},
                {"show_band", t.showBand},
                {"color", colorToJson(t.color)},
                {"color_explicit", t.colorExplicit}};
}

ConnectionType connectionTypeFromJson(const json& j) {
    ConnectionType t;
    t.id = j.value("id", newId("ctype"));
    t.name = j.value("name", "Verbindung");
    t.description = j.value("description", "");
    if (j.contains("roles") && j["roles"].is_array()) {
        for (auto& r : j["roles"]) {
            ConnectionRole role;
            role.name = r.value("name", "Rolle");
            if (r.contains("groups")) {
                for (auto& g : r["groups"]) role.allowedGroups.push_back(g.get<std::string>());
            }
            t.roles.push_back(role);
        }
    }
    if (t.roles.size() < 2) {
        while (t.roles.size() < 2) t.roles.push_back({t.roles.empty() ? "A" : "B", {}});
    }
    t.temporal = j.value("temporal", false);
    t.exclusive = j.value("exclusive", true);
    t.bandRole = j.value("band_role", 0);
    t.labelRole = j.value("label_role", 1);
    t.showBand = j.value("show_band", true);
    t.color = colorFromJson(j.value("color", json()), ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
    t.colorExplicit = j.value("color_explicit", false);
    return t;
}

json connectionToJson(const Connection& c) {
    return json{{"id", c.id},
                {"type_id", c.typeId},
                {"members", c.members},
                {"description", c.description},
                {"has_start", c.hasStart},
                {"start", c.startTime},
                {"has_end", c.hasEnd},
                {"end", c.endTime},
                {"start_text", c.hasStart ? formatStoryTime(c.startTime) : std::string()},
                {"end_text", c.hasEnd ? formatStoryTime(c.endTime) : std::string()},
                {"block", c.blockId}};
}

// Nimmt auch das alte Format (source/target/type als Text) entgegen; der Typ
// wird dann spaeter anhand des Namens erzeugt.
Connection connectionFromJson(const json& j, std::string* legacyTypeName) {
    Connection c;
    c.id = j.value("id", newId("conn"));
    c.typeId = j.value("type_id", "");
    c.description = j.value("description", "");
    c.blockId = j.value("block", "");
    if (j.contains("members") && j["members"].is_array()) {
        for (auto& m : j["members"]) c.members.push_back(m.get<std::string>());
    }
    if (c.members.empty()) {
        const std::string src = j.value("source", "");
        const std::string dst = j.value("target", "");
        if (!src.empty()) c.members.push_back(src);
        if (!dst.empty()) c.members.push_back(dst);
    }
    c.hasStart = j.value("has_start", false);
    c.startTime = j.value("start", 0LL);
    c.hasEnd = j.value("has_end", false);
    c.endTime = j.value("end", 0LL);
    if (!c.hasStart) {
        const std::string text = j.value("start_date", "");
        long long parsed = 0;
        if (!text.empty() && parseStoryTime(text, &parsed)) {
            c.hasStart = true;
            c.startTime = parsed;
        }
    }
    if (!c.hasEnd) {
        const std::string text = j.value("end_date", "");
        long long parsed = 0;
        if (!text.empty() && parseStoryTime(text, &parsed)) {
            c.hasEnd = true;
            c.endTime = parsed;
        }
    }
    if (legacyTypeName && c.typeId.empty()) *legacyTypeName = j.value("type", "related_to");
    return c;
}

json elementToJson(const Element& e) {
    json own = json::array();
    for (const FieldDef& f : e.ownFields) own.push_back(fieldToJson(f));
    return json{{"id", e.id},
                {"name", e.name},
                {"group", e.groupId},
                {"file", e.filePath},
                {"body", e.body},
                {"order", e.fieldOrder},
                {"own_fields", own},
                {"values", e.values}};
}

Element elementFromJson(const json& j) {
    Element e;
    e.id = j.value("id", newId("el"));
    e.name = j.value("name", "Element");
    e.groupId = j.value("group", "");
    e.filePath = j.value("file", "");
    e.body = j.value("body", "");
    if (j.contains("order")) {
        for (auto& o : j["order"]) e.fieldOrder.push_back(o.get<std::string>());
    }
    if (j.contains("own_fields") && j["own_fields"].is_array()) {
        for (auto& f : j["own_fields"]) e.ownFields.push_back(fieldFromJson(f));
    }
    if (j.contains("values") && j["values"].is_object()) {
        for (auto it = j["values"].begin(); it != j["values"].end(); ++it)
            e.values[it.key()] = it.value().get<std::string>();
    }
    return e;
}

std::string sanitizeFileName(const std::string& name) {
    std::string out;
    for (char c : name) {
        if (std::string("\\/:*?\"<>|").find(c) != std::string::npos)
            out += '_';
        else
            out += c;
    }
    out = trim(out);
    if (out.empty()) out = "Unbenannt";
    return out;
}

}  // namespace

std::string absolutePath(const Project& p, const std::string& relative) {
    if (relative.empty()) return p.vaultPath;
    return p.vaultPath + "/" + relative;
}

std::string elementRelativePath(const Project& p, const Element& el) {
    std::string groupDir = p.groupPath(el.groupId);
    std::string file = sanitizeFileName(el.name) + ".md";
    return groupDir.empty() ? file : groupDir + "/" + file;
}

std::string elementMarkdown(const Project& p, const Element& el) {
    std::ostringstream os;
    os << "# " << el.name << "\n\n";
    os << "<!-- story-editor: id=" << el.id << "; group=" << p.groupPath(el.groupId) << " -->\n";
    os << managedNote() << "\n\n";

    os << "## " << sectionFields() << "\n";
    std::vector<std::string> order = el.fieldOrder;
    for (const auto& kv : el.values) {
        if (std::find(order.begin(), order.end(), kv.first) == order.end()) order.push_back(kv.first);
    }
    for (const std::string& key : order) {
        auto it = el.values.find(key);
        if (it == el.values.end()) continue;
        os << "- " << key << ": " << escapeInline(it->second) << "\n";
    }
    os << "\n## " << sectionText() << "\n" << el.body << "\n";

    std::vector<const Connection*> conns = p.connectionsForElement(el.id);
    if (!conns.empty()) {
        os << "\n## " << sectionRelations() << "\n";
        for (const Connection* c : conns) {
            os << "- " << p.connectionTypeName(*c);
            for (const std::string& m : c->members) {
                if (m == el.id) continue;
                os << " -> [[" << p.displayName(m) << "]]";
            }
            if (c->hasStart) os << "  (" << formatStoryTime(c->startTime);
            if (c->hasStart && c->hasEnd) os << " - " << formatStoryTime(c->endTime);
            if (c->hasStart) os << ")";
            os << "\n";
        }
    }

    std::vector<const Action*> acts = p.actionsForElement(el.id);
    if (!acts.empty()) {
        os << "\n## " << sectionActions() << "\n";
        for (const Action* a : acts) {
            os << "- " << formatStoryTime(p.resolveActionTime(*a)) << " - " << a->title << "\n";
        }
    }
    return os.str();
}

bool parseElementMarkdown(const std::string& text, std::string* outName,
                          std::vector<std::pair<std::string, std::string>>* outFields,
                          std::string* outBody, std::string* outId, std::string* outGroupPath) {
    if (outName) outName->clear();
    if (outFields) outFields->clear();
    if (outBody) outBody->clear();
    if (outId) outId->clear();
    if (outGroupPath) outGroupPath->clear();

    std::istringstream is(text);
    std::string line;
    std::string section;
    std::ostringstream body;
    bool bodyStarted = false;

    while (std::getline(is, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();

        if (line.rfind("# ", 0) == 0 && line.rfind("## ", 0) != 0) {
            if (outName) *outName = trim(line.substr(2));
            continue;
        }
        if (line.rfind("<!-- story-editor:", 0) == 0) {
            std::string meta = line.substr(std::string("<!-- story-editor:").size());
            size_t end = meta.find("-->");
            if (end != std::string::npos) meta = meta.substr(0, end);
            for (const std::string& part : splitString(meta, ';')) {
                std::string t = trim(part);
                size_t eq = t.find('=');
                if (eq == std::string::npos) continue;
                std::string key = trim(t.substr(0, eq));
                std::string value = trim(t.substr(eq + 1));
                if (key == "id" && outId) *outId = value;
                if (key == "group" && outGroupPath) *outGroupPath = value;
            }
            continue;
        }
        if (line.rfind("## ", 0) == 0) {
            section = trim(line.substr(3));
            continue;
        }
        if (section == "Felder" || section == "Fields") {
            if (line.rfind("- ", 0) != 0) continue;
            std::string rest = line.substr(2);
            size_t colon = rest.find(':');
            if (colon == std::string::npos) continue;
            std::string key = trim(rest.substr(0, colon));
            std::string value = trim(rest.substr(colon + 1));
            if (outFields) outFields->push_back({key, unescapeInline(value)});
        } else if (section == "Text") {
            if (!bodyStarted && trim(line).empty()) continue;
            bodyStarted = true;
            body << line << "\n";
        }
    }
    if (outBody) {
        std::string b = body.str();
        while (!b.empty() && (b.back() == '\n' || b.back() == ' ')) b.pop_back();
        *outBody = b;
    }
    return true;
}

bool ensureGroupDirs(const Project& p, std::string* err) {
    for (const Group& g : p.groups) {
        if (!platform::ensureDir(absolutePath(p, p.groupPath(g.id)), err)) return false;
    }
    const char* extra[] = {"Timeline", "Actions", "Connections", "Assets/Images", "Assets/Documents"};
    for (const char* dir : extra) {
        if (!platform::ensureDir(absolutePath(p, dir), err)) return false;
    }
    return true;
}

bool saveMetadata(const Project& p, std::string* err) {
    json j;
    j["version"] = 1;
    j["project"] = {{"name", p.name}};
    j["groups"] = json::array();
    for (const Group& g : p.groups) j["groups"].push_back(groupToJson(g));
    j["action_types"] = p.actionTypes;
    j["connection_types"] = json::array();
    for (const ConnectionType& t : p.connectionTypes)
        j["connection_types"].push_back(connectionTypeToJson(t));
    j["storylines"] = p.storylines;
    j["node_positions"] = json::object();
    for (const auto& kv : p.nodePositions) {
        j["node_positions"][kv.first] = json::array({kv.second.x, kv.second.y});
    }
    // Felder, die nur an einem einzelnen Element haengen - die .md-Datei kennt
    // nur den Wert, der Typ steht hier.
    j["element_fields"] = json::object();
    for (const Element& el : p.elements) {
        if (el.ownFields.empty()) continue;
        json own = json::array();
        for (const FieldDef& f : el.ownFields) own.push_back(fieldToJson(f));
        j["element_fields"][el.id] = own;
    }
    return platform::writeFile(absolutePath(p, "metadata.json"), j.dump(2), err);
}

bool saveActions(const Project& p, std::string* err) {
    json j;
    j["actions"] = json::array();
    std::vector<const Action*> sorted = p.sortedActions();
    for (const Action* a : sorted) j["actions"].push_back(actionToJson(*a));
    if (!platform::writeFile(absolutePath(p, "Actions/actions.json"), j.dump(2), err)) return false;

    // human readable overview for obsidian (generated, never read back)
    std::ostringstream os;
    os << "# Timeline\n\n";
    long long lastDay = -1;
    for (const Action* a : sorted) {
        long long t = p.resolveActionTime(*a);
        if (dayOf(t) != lastDay) {
            lastDay = dayOf(t);
            os << "\n## " << formatDayHeadline(t) << "\n";
        }
        os << "- **" << formatStoryTime(t) << "** " << a->title;
        if (!a->elementIds.empty()) {
            os << " (";
            for (size_t i = 0; i < a->elementIds.size(); ++i) {
                if (i) os << ", ";
                os << p.displayName(a->elementIds[i]);
            }
            os << ")";
        }
        os << "\n";
    }
    return platform::writeFile(absolutePath(p, "Timeline/events.md"), os.str(), err);
}

bool saveConnections(const Project& p, std::string* err) {
    json j;
    j["connections"] = json::array();
    for (const Connection& c : p.connections) j["connections"].push_back(connectionToJson(c));
    j["blocks"] = json::array();
    for (const Block& b : p.blocks) {
        j["blocks"].push_back(
            {{"id", b.id}, {"name", b.name}, {"description", b.description}, {"collapsed", b.collapsed}});
    }
    return platform::writeFile(absolutePath(p, "Connections/relationships.json"), j.dump(2), err);
}

bool saveElement(const Project& p, Element& el, std::string* err) {
    std::string target = elementRelativePath(p, el);
    if (!el.filePath.empty() && el.filePath != target) {
        std::error_code ec;
        fs::remove(platform::fsPath(absolutePath(p, el.filePath)), ec);
    }
    el.filePath = target;
    return platform::writeFile(absolutePath(p, target), elementMarkdown(p, el), err);
}

void deleteElementFile(const Project& p, const Element& el) {
    if (el.filePath.empty()) return;
    std::error_code ec;
    fs::remove(platform::fsPath(absolutePath(p, el.filePath)), ec);
}

bool saveAll(Project& p, std::string* err) {
    if (p.vaultPath.empty()) {
        if (err) *err = "Kein Projekt geoeffnet.";
        return false;
    }
    if (!ensureGroupDirs(p, err)) return false;
    if (!saveMetadata(p, err)) return false;
    if (!saveActions(p, err)) return false;
    if (!saveConnections(p, err)) return false;
    for (Element& el : p.elements) {
        if (!saveElement(p, el, err)) return false;
    }
    return true;
}

bool createVault(const std::string& path, const std::string& name, Project& p, std::string* err) {
    if (!platform::ensureDir(path, err)) return false;
    p.clear();
    p.name = name;
    p.vaultPath = path;
    p.loaded = true;

    Group& characters = p.addGroup("Characters", "");
    characters.fields.push_back({"name", FieldType::Text, true, "", "Name der Figur", "", {}});
    characters.fields.push_back({"age", FieldType::Integer, false, "0", "Alter in Jahren", "", {}});
    characters.fields.push_back(
        {"description", FieldType::Text, false, "", "Kurzbeschreibung", "", {}});
    std::string charactersId = characters.id;
    p.addGroup("Main", charactersId);
    p.addGroup("NPCs", charactersId);

    Group& locations = p.addGroup("Locations", "");
    locations.fields.push_back({"name", FieldType::Text, true, "", "Name des Ortes", "", {}});
    locations.fields.push_back({"description", FieldType::Text, false, "", "Beschreibung", "", {}});

    Group& objects = p.addGroup("Objects", "");
    objects.fields.push_back({"name", FieldType::Text, true, "", "Name des Objekts", "", {}});
    objects.fields.push_back({"description", FieldType::Text, false, "", "Beschreibung", "", {}});

    return saveAll(p, err);
}

bool load(const std::string& path, Project& p, std::string* err) {
    std::string metaText;
    if (!platform::readFile(path + "/metadata.json", &metaText)) {
        if (err) *err = "metadata.json nicht gefunden in " + path;
        return false;
    }
    json meta;
    try {
        meta = json::parse(metaText);
    } catch (const std::exception& e) {
        if (err) *err = std::string("metadata.json ist beschaedigt: ") + e.what();
        return false;
    }

    p.clear();
    p.vaultPath = path;
    p.name = meta.contains("project") ? meta["project"].value("name", "Story") : "Story";
    if (meta.contains("groups")) {
        for (auto& g : meta["groups"]) p.groups.push_back(groupFromJson(g));
    }
    if (meta.contains("action_types") && meta["action_types"].is_array()) {
        p.actionTypes.clear();
        for (auto& t : meta["action_types"]) p.actionTypes.push_back(t.get<std::string>());
    }
    if (meta.contains("connection_types") && meta["connection_types"].is_array()) {
        p.connectionTypes.clear();
        for (auto& t : meta["connection_types"]) {
            if (t.is_string())
                p.ensureConnectionType(t.get<std::string>());  // altes Format: nur Namen
            else
                p.connectionTypes.push_back(connectionTypeFromJson(t));
        }
    }
    if (meta.contains("storylines") && meta["storylines"].is_array()) {
        p.storylines.clear();
        for (auto& t : meta["storylines"]) p.storylines.push_back(t.get<std::string>());
    }
    if (meta.contains("node_positions") && meta["node_positions"].is_object()) {
        for (auto it = meta["node_positions"].begin(); it != meta["node_positions"].end(); ++it) {
            if (it.value().is_array() && it.value().size() >= 2)
                p.nodePositions[it.key()] =
                    ImVec2(it.value()[0].get<float>(), it.value()[1].get<float>());
        }
    }

    std::map<std::string, std::vector<FieldDef>> elementFields;
    if (meta.contains("element_fields") && meta["element_fields"].is_object()) {
        for (auto it = meta["element_fields"].begin(); it != meta["element_fields"].end(); ++it) {
            std::vector<FieldDef> defs;
            for (auto& f : it.value()) defs.push_back(fieldFromJson(f));
            elementFields[it.key()] = defs;
        }
    }

    // elements live inside the group folders
    for (const Group& g : p.groups) {
        std::string dir = absolutePath(p, p.groupPath(g.id));
        std::error_code ec;
        if (!fs::exists(platform::fsPath(dir), ec)) continue;
        for (const auto& entry : fs::directory_iterator(platform::fsPath(dir), ec)) {
            if (entry.is_directory()) continue;
            if (entry.path().extension() != L".md") continue;
            std::string file = platform::pathToUtf8(entry.path());
            std::string text;
            if (!platform::readFile(file, &text)) continue;

            std::string name, body, id, groupPath;
            std::vector<std::pair<std::string, std::string>> fields;
            parseElementMarkdown(text, &name, &fields, &body, &id, &groupPath);
            Element el;
            el.id = id.empty() ? newId("el") : id;
            el.name = name.empty() ? platform::pathToUtf8(entry.path().stem()) : name;
            el.groupId = g.id;
            el.body = body;
            el.filePath = p.groupPath(g.id) + "/" + platform::pathToUtf8(entry.path().filename());
            for (auto& kv : fields) {
                el.values[kv.first] = kv.second;
                el.fieldOrder.push_back(kv.first);
            }
            p.elements.push_back(el);
        }
    }
    for (Element& el : p.elements) {
        auto it = elementFields.find(el.id);
        if (it != elementFields.end()) el.ownFields = it->second;
        p.syncElementFields(el);
    }

    std::string actionsText;
    if (platform::readFile(path + "/Actions/actions.json", &actionsText)) {
        try {
            json j = json::parse(actionsText);
            if (j.contains("actions")) {
                for (auto& a : j["actions"]) p.actions.push_back(actionFromJson(a));
            }
        } catch (...) {
            if (err) *err = "Actions/actions.json konnte nicht gelesen werden.";
        }
    }

    std::string connText;
    if (platform::readFile(path + "/Connections/relationships.json", &connText)) {
        try {
            json j = json::parse(connText);
            if (j.contains("connections")) {
                for (auto& c : j["connections"]) {
                    std::string legacyType;
                    Connection conn = connectionFromJson(c, &legacyType);
                    if (conn.typeId.empty()) {
                        // Altbestand: Typ anhand des Namens anlegen bzw. finden
                        ConnectionType& t = p.ensureConnectionType(
                            legacyType.empty() ? std::string("related_to") : legacyType);
                        conn.typeId = t.id;
                    }
                    p.connections.push_back(conn);
                }
            }
            if (j.contains("blocks")) {
                for (auto& b : j["blocks"]) {
                    Block blk;
                    blk.id = b.value("id", newId("blk"));
                    blk.name = b.value("name", "Block");
                    blk.description = b.value("description", "");
                    blk.collapsed = b.value("collapsed", false);
                    p.blocks.push_back(blk);
                }
            }
        } catch (...) {
            if (err) *err = "Connections/relationships.json konnte nicht gelesen werden.";
        }
    }

    // whatever the data actually uses belongs into the selectable lists, even if
    // metadata.json is older than the entry (or was written by an older version)
    auto mergeInto = [](std::vector<std::string>& list, const std::string& value) {
        if (value.empty()) return;
        if (std::find(list.begin(), list.end(), value) == list.end()) list.push_back(value);
    };
    for (const Action& a : p.actions) {
        mergeInto(p.actionTypes, a.type);
        mergeInto(p.storylines, a.storyline);
    }
    for (Connection& c : p.connections) {
        if (!p.connectionType(c.typeId)) c.typeId = p.ensureConnectionType("related_to").id;
    }

    p.loaded = true;
    return true;
}

json snapshot(const Project& p) {
    json j;
    j["name"] = p.name;
    j["groups"] = json::array();
    for (const Group& g : p.groups) j["groups"].push_back(groupToJson(g));
    j["elements"] = json::array();
    for (const Element& e : p.elements) j["elements"].push_back(elementToJson(e));
    j["actions"] = json::array();
    for (const Action& a : p.actions) j["actions"].push_back(actionToJson(a));
    j["connections"] = json::array();
    for (const Connection& c : p.connections) j["connections"].push_back(connectionToJson(c));
    j["connection_type_defs"] = json::array();
    for (const ConnectionType& t : p.connectionTypes)
        j["connection_type_defs"].push_back(connectionTypeToJson(t));
    j["blocks"] = json::array();
    for (const Block& b : p.blocks) {
        j["blocks"].push_back(
            {{"id", b.id}, {"name", b.name}, {"description", b.description}, {"collapsed", b.collapsed}});
    }
    j["node_positions"] = json::object();
    for (const auto& kv : p.nodePositions)
        j["node_positions"][kv.first] = json::array({kv.second.x, kv.second.y});
    j["action_types"] = p.actionTypes;
    j["storylines"] = p.storylines;
    return j;
}

void restore(const json& j, Project& p) {
    p.name = j.value("name", p.name);
    p.groups.clear();
    p.elements.clear();
    p.actions.clear();
    p.connections.clear();
    p.connectionTypes.clear();
    p.blocks.clear();
    p.nodePositions.clear();
    for (auto& g : j["groups"]) p.groups.push_back(groupFromJson(g));
    for (auto& e : j["elements"]) p.elements.push_back(elementFromJson(e));
    for (auto& a : j["actions"]) p.actions.push_back(actionFromJson(a));
    if (j.contains("connection_type_defs")) {
        for (auto& t : j["connection_type_defs"])
            p.connectionTypes.push_back(connectionTypeFromJson(t));
    }
    for (auto& c : j["connections"]) {
        std::string legacyType;
        Connection conn = connectionFromJson(c, &legacyType);
        if (conn.typeId.empty()) conn.typeId = p.ensureConnectionType(legacyType).id;
        p.connections.push_back(conn);
    }
    for (auto& b : j["blocks"]) {
        Block blk;
        blk.id = b.value("id", newId("blk"));
        blk.name = b.value("name", "Block");
        blk.description = b.value("description", "");
        blk.collapsed = b.value("collapsed", false);
        p.blocks.push_back(blk);
    }
    if (j.contains("node_positions")) {
        for (auto it = j["node_positions"].begin(); it != j["node_positions"].end(); ++it) {
            if (it.value().is_array() && it.value().size() >= 2)
                p.nodePositions[it.key()] = ImVec2(it.value()[0].get<float>(), it.value()[1].get<float>());
        }
    }
    if (j.contains("action_types")) {
        p.actionTypes.clear();
        for (auto& t : j["action_types"]) p.actionTypes.push_back(t.get<std::string>());
    }

    if (j.contains("storylines")) {
        p.storylines.clear();
        for (auto& t : j["storylines"]) p.storylines.push_back(t.get<std::string>());
    }
}

}  // namespace se::vault
