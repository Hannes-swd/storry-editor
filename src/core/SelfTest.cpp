#include "core/SelfTest.h"

#include <filesystem>
#include <sstream>

#include "app/Platform.h"
#include "core/Project.h"
#include "core/StoryTime.h"
#include "core/VaultIO.h"

namespace fs = std::filesystem;

namespace se {
namespace {

struct Report {
    std::ostringstream os;
    int failed = 0;
    int passed = 0;

    void check(bool condition, const std::string& what) {
        if (condition) {
            ++passed;
            os << "[ ok ] " << what << "\n";
        } else {
            ++failed;
            os << "[FAIL] " << what << "\n";
        }
    }
};

void testStoryTime(Report& r) {
    long long t = 0;
    r.check(parseStoryTime("Tag 5, 14:30", &t) && t == 4 * kMinutesPerDay + 14 * 60 + 30,
            "parseStoryTime('Tag 5, 14:30')");
    r.check(parseStoryTime("Day 1", &t) && t == 0, "parseStoryTime('Day 1')");
    r.check(parseStoryTime("Jahr 2, Monat 3, Tag 15", &t) &&
                t == (360 + 60 + 14) * kMinutesPerDay,
            "parseStoryTime('Jahr 2, Monat 3, Tag 15')");
    r.check(!parseStoryTime("", &t), "parseStoryTime('') fails");
    r.check(formatStoryTime(4 * kMinutesPerDay + 14 * 60 + 30) == "Tag 5, 14:30", "formatStoryTime");
    r.check(formatDuration(3 * kMinutesPerDay) == "3 Tage", "formatDuration days");
    r.check(formatDuration(120) == "2 Stunden", "formatDuration hours");
    long long round = 0;
    r.check(parseStoryTime(formatStoryTime(12345), &round) && round == 12345,
            "formatStoryTime/parseStoryTime round trip");
}

void testTemplateInheritance(Report& r) {
    Project p;
    Group& characters = p.addGroup("Characters", "");
    characters.fields.push_back({"name", FieldType::Text, true, "", "", "", {}});
    characters.fields.push_back({"age", FieldType::Integer, true, "0", "", "", {}});
    Group& main = p.addGroup("Main", characters.id);
    main.fields.push_back({"backstory", FieldType::Text, false, "", "", "", {}});

    std::vector<FieldDef> fields = p.effectiveFields(main.id);
    r.check(fields.size() == 3, "sub group inherits parent fields");
    r.check(fields[0].name == "name" && fields[2].name == "backstory", "inherited order");

    Element& alice = p.addElement("Alice", main.id);
    r.check(alice.values.count("age") == 1 && alice.values["age"] == "0", "defaults applied");
    r.check(p.elementPath(alice.id) == "Characters/Main/Alice", "element path");

    characters.fields.push_back({"birthday", FieldType::Date, false, "", "", "", {}});
    p.applyTemplateToElements(characters.id);
    r.check(p.element(alice.id)->values.count("birthday") == 1,
            "new template field reaches existing elements");
    r.check(p.element(alice.id)->values["age"] == "0", "existing values are kept");
}

void testMutations(Report& r) {
    Project p;
    Group& g = p.addGroup("Characters", "");
    g.fields.push_back({"age", FieldType::Integer, false, "27", "", "", {}});
    Element& alice = p.addElement("Alice", g.id);
    Action& birthday = p.addAction("Alice's Birthday", 10 * kMinutesPerDay);
    birthday.elementIds.push_back(alice.id);
    birthday.mutations.push_back({alice.id, "age", "27", "28"});

    r.check(p.valueAt(alice.id, "age", 5 * kMinutesPerDay) == "27", "value before mutation");
    r.check(p.valueAt(alice.id, "age", 12 * kMinutesPerDay) == "28", "value after mutation");
    r.check(p.actionsForElement(alice.id).size() == 1, "actions for element");

    Action& later = p.addAction("Spaeter", 0);
    later.useRelative = true;
    later.relativeToId = birthday.id;
    later.relativeOffset = 2 * kMinutesPerHour;
    r.check(p.resolveActionTime(later) == 10 * kMinutesPerDay + 120, "relative time resolution");
}

void testMarkdownRoundTrip(Report& r) {
    Project p;
    p.vaultPath = "C:/tmp/none";
    Group& g = p.addGroup("Characters", "");
    g.fields.push_back({"name", FieldType::Text, true, "", "", "", {}});
    g.fields.push_back({"description", FieldType::Text, false, "", "", "", {}});
    Element& el = p.addElement("Alice", g.id);
    el.values["name"] = "Alice";
    el.values["description"] = "Zeile 1\nZeile 2";
    el.body = "Freitext\nmit Zeilen";

    std::string md = vault::elementMarkdown(p, el);
    std::string name, body, id, groupPath;
    std::vector<std::pair<std::string, std::string>> fields;
    vault::parseElementMarkdown(md, &name, &fields, &body, &id, &groupPath);

    r.check(name == "Alice", "markdown keeps the name");
    r.check(id == el.id, "markdown keeps the id");
    r.check(groupPath == "Characters", "markdown keeps the group path");
    bool descOk = false;
    for (auto& kv : fields) {
        if (kv.first == "description" && kv.second == "Zeile 1\nZeile 2") descOk = true;
    }
    r.check(descOk, "multi line field survives the round trip");
    r.check(body == "Freitext\nmit Zeilen", "free text survives the round trip");
}

void testVaultRoundTrip(Report& r) {
    std::error_code ec;
    fs::path tmp = fs::temp_directory_path(ec) / "story_editor_selftest";
    fs::remove_all(tmp, ec);
    std::string vaultPath = platform::pathToUtf8(tmp);

    Project p;
    std::string err;
    bool created = vault::createVault(vaultPath, "Testprojekt", p, &err);
    r.check(created, "createVault: " + err);
    if (!created) return;

    Group* main = p.findGroupByPath("Characters/Main");
    r.check(main != nullptr, "default structure contains Characters/Main");
    if (!main) return;

    Element& alice = p.addElement("Alice", main->id);
    alice.values["name"] = "Alice";
    alice.values["age"] = "28";
    alice.values["description"] = "Eine mutige Ritterin.";

    // ein Feld, das es nur bei Alice gibt
    FieldDef nickname;
    nickname.name = "nickname";
    nickname.type = FieldType::Text;
    p.addOwnField(alice, nickname);
    alice.values["nickname"] = "Die Ritterin";
    FieldDef luck;
    luck.name = "luck";
    luck.type = FieldType::Integer;
    luck.defaultValue = "7";
    p.addOwnField(alice, luck);

    Group* objects = p.findGroupByPath("Objects");
    Element& sword = p.addElement("Sword", objects->id);

    Action& a = p.addAction("Alice erhaelt das Schwert", 4 * kMinutesPerDay + 14 * 60);
    a.elementIds.push_back(alice.id);
    a.elementIds.push_back(sword.id);
    a.tags.push_back("Discovery");
    a.mutations.push_back({alice.id, "age", "27", "28"});

    Connection& conn = p.addConnection(alice.id, sword.id, "owns");
    conn.description = "Alice besitzt das Schwert.";

    r.check(vault::saveAll(p, &err), "saveAll: " + err);
    r.check(fs::exists(platform::fsPath(vaultPath + "/Characters/Main/Alice.md")), "Alice.md written");
    r.check(fs::exists(platform::fsPath(vaultPath + "/Actions/actions.json")), "actions.json written");
    r.check(fs::exists(platform::fsPath(vaultPath + "/Connections/relationships.json")),
            "relationships.json written");
    r.check(fs::exists(platform::fsPath(vaultPath + "/Timeline/events.md")), "events.md written");

    Project loaded;
    r.check(vault::load(vaultPath, loaded, &err), "load: " + err);
    r.check(loaded.name == "Testprojekt", "project name restored");
    r.check(loaded.groups.size() == p.groups.size(), "groups restored");
    r.check(loaded.elements.size() == p.elements.size(), "elements restored");
    r.check(loaded.actions.size() == 1, "actions restored");
    r.check(loaded.connections.size() == 1, "connections restored");

    const Element* aliceLoaded = loaded.findElementByPath("Characters/Main/Alice");
    r.check(aliceLoaded != nullptr, "Alice found by path after load");
    if (aliceLoaded) {
        r.check(aliceLoaded->id == alice.id, "element id is stable");
        r.check(aliceLoaded->values.at("age") == "28", "field value restored");
        r.check(loaded.valueAt(aliceLoaded->id, "age", 0) == "28", "valueAt before mutation");
        r.check(aliceLoaded->ownFields.size() == 2, "per element fields restored");
        r.check(loaded.isOwnField(*aliceLoaded, "luck"), "own field recognised");
        r.check(aliceLoaded->values.count("nickname") == 1 &&
                    aliceLoaded->values.at("nickname") == "Die Ritterin",
                "per element value restored");
        r.check(aliceLoaded->values.count("luck") == 1 && aliceLoaded->values.at("luck") == "7",
                "per element default applied");
        bool typed = false;
        for (const FieldDef& f : aliceLoaded->ownFields) {
            if (f.name == "luck" && f.type == FieldType::Integer) typed = true;
        }
        r.check(typed, "per element field keeps its type");
        const Element* bobLoaded = loaded.findElementByPath("Objects/Sword");
        r.check(bobLoaded && bobLoaded->values.count("nickname") == 0,
                "per element field stays on that element");
    }

    // undo snapshot round trip
    nlohmann::json snap = vault::snapshot(loaded);
    size_t before = loaded.actions.size();
    loaded.actions.clear();
    vault::restore(snap, loaded);
    r.check(loaded.actions.size() == before, "snapshot/restore keeps actions");

    // rename moves the file
    Element* target = loaded.element(alice.id);
    if (target) {
        target->name = "Alicia";
        vault::saveElement(loaded, *target, &err);
        r.check(fs::exists(platform::fsPath(vaultPath + "/Characters/Main/Alicia.md")),
                "renamed file created");
        r.check(!fs::exists(platform::fsPath(vaultPath + "/Characters/Main/Alice.md")),
                "old file removed");
    }

    fs::remove_all(tmp, ec);
}

}  // namespace

int createDemoProject(const std::string& vaultPath) {
    Project p;
    std::string err;
    if (!vault::createVault(vaultPath, "Demo Story", p, &err)) return 1;

    Group* main = p.findGroupByPath("Characters/Main");
    Group* npcs = p.findGroupByPath("Characters/NPCs");
    Group* locations = p.findGroupByPath("Locations");
    Group* objects = p.findGroupByPath("Objects");
    if (!main || !npcs || !locations || !objects) return 1;

    main->fields.push_back({"backstory", FieldType::Text, false, "", "Vorgeschichte", "", {}});
    main->fields.push_back({"status", FieldType::Enum, false, "Alive", "Zustand", "",
                            {"Alive", "Injured", "Missing", "Dead"}});

    auto makeElement = [&](Group* group, const std::string& name,
                           const std::vector<std::pair<std::string, std::string>>& values) {
        Element& el = p.addElement(name, group->id);
        el.values["name"] = name;
        for (const auto& kv : values) el.values[kv.first] = kv.second;
        p.syncElementFields(el);
        return el.id;
    };

    std::string alice = makeElement(main, "Alice",
                                    {{"age", "27"},
                                     {"description", "Eine mutige Ritterin mit unruhiger Vergangenheit."},
                                     {"status", "Alive"}});
    std::string bob = makeElement(main, "Bob",
                                  {{"age", "31"}, {"description", "Haendler am Markt."}, {"status", "Alive"}});
    std::string guard = makeElement(npcs, "Guard", {{"age", "40"}, {"description", "Wache am Tor."}});
    std::string castle = makeElement(locations, "Castle", {{"description", "Die alte Burg auf dem Huegel."}});
    std::string forest = makeElement(locations, "Forest", {{"description", "Dichter Wald im Norden."}});
    std::string sword = makeElement(objects, "Sword", {{"description", "Ein uraltes, schwach leuchtendes Schwert."}});

    auto makeAction = [&](const std::string& title, long long time, const std::string& type,
                          const std::vector<std::string>& els, const std::string& desc, bool major) {
        Action& a = p.addAction(title, time);
        a.type = type;
        a.description = desc;
        a.elementIds = els;
        a.major = major;
        a.storyline = "Main Quest";
        return a.id;
    };

    makeAction("Alice kehrt zurueck", 9 * kMinutesPerHour, "Scene Change", {alice, castle},
               "Alice kommt nach Jahren in ihre Heimatstadt zurueck.", true);
    makeAction("Alice trifft Bob am Markt", 11 * kMinutesPerHour, "Dialogue", {alice, bob},
               "Die beiden erkennen sich erst nach einem Moment.", false);
    makeAction("Bob erzaehlt vom Drachen", 11 * kMinutesPerHour + 20, "Dialogue", {alice, bob},
               "Bob berichtet von Feuern im Norden.", false);

    std::string swordAction =
        makeAction("Alice erhaelt das Schwert", 4 * kMinutesPerDay + 14 * kMinutesPerHour, "Discovery",
                   {alice, sword, forest}, "Alice findet ein uraltes Schwert in einer Ruine.", true);
    if (Action* a = p.action(swordAction)) {
        a->tags = {"Magic", "Discovery"};
        a->mutations.push_back({alice, "description", "Eine mutige Ritterin mit unruhiger Vergangenheit.",
                                "Eine Ritterin mit einem magischen Schwert."});
    }

    std::string birthday = makeAction("Alice's Geburtstag", 10 * kMinutesPerDay + 8 * kMinutesPerHour,
                                      "Action", {alice}, "Alice wird ein Jahr aelter.", false);
    if (Action* a = p.action(birthday)) a->mutations.push_back({alice, "age", "27", "28"});

    std::string attack = makeAction("Drache greift die Burg an", 40 * kMinutesPerDay + 6 * kMinutesPerHour,
                                    "Conflict", {castle, guard, bob},
                                    "Feuer faellt auf die Burg, die Wachen fliehen.", true);
    if (Action* a = p.action(attack)) a->mutations.push_back({bob, "status", "Alive", "Injured"});

    p.addConnection(alice, bob, "married_to").description = "Alice und Bob sind verheiratet.";
    p.addConnection(alice, sword, "owns").description = "Seit Tag 5.";
    p.addConnection(guard, castle, "guards").description = "Wache am Haupttor.";

    return vault::saveAll(p, &err) ? 0 : 1;
}

int runSelfTest(const std::string& reportPath) {
    Report r;
    r.os << "Story Editor selftest\n=====================\n";
    testStoryTime(r);
    testTemplateInheritance(r);
    testMutations(r);
    testMarkdownRoundTrip(r);
    testVaultRoundTrip(r);
    r.os << "---------------------\n"
         << r.passed << " ok, " << r.failed << " fehlgeschlagen\n";

    std::string text = r.os.str();
    if (!reportPath.empty()) {
        std::string err;
        platform::writeFile(reportPath, text, &err);
    }
    return r.failed == 0 ? 0 : 1;
}

}  // namespace se
