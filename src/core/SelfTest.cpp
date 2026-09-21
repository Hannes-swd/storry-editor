#include "core/SelfTest.h"

#include <filesystem>
#include <sstream>

#include "app/Platform.h"
#include "core/Manuscript.h"
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

    ConnectionType& owns = p.ensureConnectionType("owns");
    Connection& conn = p.addConnection(owns.id, {alice.id, sword.id});
    conn.description = "Alice besitzt das Schwert.";

    // zeitlicher, exklusiver Typ: "ist an Ort"
    ConnectionType& atPlace = p.addConnectionType("ist an Ort");
    atPlace.temporal = true;
    atPlace.exclusive = true;
    atPlace.roles[0].name = "Wer";
    atPlace.roles[1].name = "Wo";
    Group* locations = p.findGroupByPath("Locations");
    Element& castle = p.addElement("Castle", locations->id);
    Element& forest = p.addElement("Forest", locations->id);
    Connection& first = p.addConnection(atPlace.id, {alice.id, forest.id});
    first.hasStart = true;
    first.startTime = 1 * kMinutesPerDay;
    Connection& second = p.addConnection(atPlace.id, {alice.id, castle.id});
    second.hasStart = true;
    second.startTime = 5 * kMinutesPerDay;
    p.applyExclusivity(second);

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
    r.check(loaded.connectionTypes.size() == p.connectionTypes.size(), "connection types restored");
    r.check(loaded.actions.size() == 1, "actions restored");
    r.check(loaded.connections.size() == p.connections.size(), "connections restored");

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

    // zeitliche Verbindungen
    {
        const ConnectionType* type = loaded.connectionTypeByName("ist an Ort");
        r.check(type != nullptr, "temporal connection type restored");
        if (type) {
            r.check(type->temporal && type->exclusive, "temporal flags restored");
            const Element* al = loaded.findElementByPath("Characters/Main/Alice");
            std::vector<Project::BandSegment> segs =
                al ? loaded.bandSegments(al->id, type->id) : std::vector<Project::BandSegment>();
            r.check(segs.size() == 2, "two band segments for Alice");
            if (segs.size() == 2) {
                r.check(segs[0].hasEnd && segs[0].end == segs[1].start,
                        "exclusive segment ends where the next starts");
                r.check(!segs[1].hasEnd, "last segment stays open");
                r.check(loaded.displayName(segs[1].labelElementId) == "Castle",
                        "band label is the other role");
            }
            const Connection* atForest = nullptr;
            for (const Connection& c : loaded.connections) {
                if (c.typeId == type->id && loaded.displayName(c.members[1]) == "Forest")
                    atForest = &c;
            }
            r.check(atForest && !loaded.connectionActiveAt(*atForest, 6 * kMinutesPerDay),
                    "ended connection is inactive later");
            r.check(atForest && loaded.connectionActiveAt(*atForest, 2 * kMinutesPerDay),
                    "connection active inside its range");
        }
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


// Ein Projekt aus der Zeit vor den Verbindungstypen muss weiter laden.
void testLegacyConnections(Report& r) {
    std::error_code ec;
    fs::path tmp = fs::temp_directory_path(ec) / "story_editor_legacy";
    fs::remove_all(tmp, ec);
    std::string vaultPath = platform::pathToUtf8(tmp);

    Project p;
    std::string err;
    if (!vault::createVault(vaultPath, "Alt", p, &err)) {
        r.check(false, "legacy: createVault");
        return;
    }
    Group* main = p.findGroupByPath("Characters/Main");
    Group* objects = p.findGroupByPath("Objects");
    Element& alice = p.addElement("Alice", main->id);
    Element& sword = p.addElement("Sword", objects->id);
    const std::string aliceId = alice.id;
    const std::string swordId = sword.id;
    vault::saveAll(p, &err);

    // altes Format von Hand schreiben: source/target/type als Text
    std::string legacy =
        "{\n  \"connections\": [\n    {\n      \"id\": \"conn_old\",\n"
        "      \"source\": \"" + aliceId + "\",\n"
        "      \"target\": \"" + swordId + "\",\n"
        "      \"type\": \"owns\",\n"
        "      \"description\": \"alt\",\n"
        "      \"start_date\": \"Tag 5\",\n"
        "      \"end_date\": \"\"\n    }\n  ],\n  \"blocks\": []\n}\n";
    platform::writeFile(vaultPath + "/Connections/relationships.json", legacy, &err);

    Project loaded;
    r.check(vault::load(vaultPath, loaded, &err), "legacy: load: " + err);
    r.check(loaded.connections.size() == 1, "legacy: connection kept");
    if (!loaded.connections.empty()) {
        const Connection& c = loaded.connections.front();
        r.check(!c.typeId.empty(), "legacy: type created from the name");
        r.check(loaded.connectionTypeName(c) == "owns", "legacy: type name kept");
        r.check(c.members.size() == 2 && c.members[0] == aliceId && c.members[1] == swordId,
                "legacy: source/target became roles");
        r.check(c.hasStart && c.startTime == 4 * kMinutesPerDay, "legacy: start_date parsed");
        r.check(!c.hasEnd, "legacy: empty end stays open");
    }
    fs::remove_all(tmp, ec);
}


// Marken im Manuskript: Verweise, Werte zur richtigen Zeit, Aktionen.
void testManuscript(Report& r) {
    Project p;
    Group& chars = p.addGroup("Characters", "");
    chars.fields.push_back({"age", FieldType::Integer, false, "27", "", "", {}});
    Element& alice = p.addElement("Alice", chars.id);
    alice.values["age"] = "27";
    Action& birthday = p.addAction("Geburtstag", 10 * kMinutesPerDay);
    birthday.mutations.push_back({alice.id, "age", "27", "28"});

    const std::string text =
        "## Kapitel 1\n"
        "#Tag 2, 09:00\n"
        "@Alice ist @Alice.age Jahre alt.\n\n"
        "#Tag 12, 09:00\n"
        "Jetzt ist @Alice schon @Alice.age. !act:" + birthday.id + "\n";

    std::vector<ManuscriptToken> tokens = parseManuscript(p, text);
    size_t elements = 0, values = 0, times = 0, actions = 0, headings = 0;
    for (const ManuscriptToken& t : tokens) {
        if (t.kind == ManuscriptToken::Kind::Element) ++elements;
        if (t.kind == ManuscriptToken::Kind::Value) ++values;
        if (t.kind == ManuscriptToken::Kind::Time) ++times;
        if (t.kind == ManuscriptToken::Kind::Action) ++actions;
        if (t.kind == ManuscriptToken::Kind::Heading) ++headings;
    }
    r.check(elements == 2, "manuscript: two element references");
    r.check(values == 2, "manuscript: two value references");
    r.check(times == 2, "manuscript: two time markers");
    r.check(actions == 1, "manuscript: action marker");
    r.check(headings == 1, "manuscript: heading");

    const std::string rendered = renderManuscript(p, text);
    r.check(rendered.find("Alice ist 27 Jahre alt") != std::string::npos,
            "manuscript: value before the mutation");
    r.check(rendered.find("Alice schon 28") != std::string::npos,
            "manuscript: value after the mutation");
    r.check(rendered.find("Geburtstag") != std::string::npos, "manuscript: action title inserted");
    r.check(rendered.find("@") == std::string::npos, "manuscript: no markers left after rendering");
    r.check(rendered.find("Tag 2") == std::string::npos,
            "manuscript: time marker invisible in the finished text");
    r.check(rendered.find("#") == std::string::npos ||
                rendered.find("## Kapitel") != std::string::npos,
            "manuscript: only headings keep their hash");

    // Erzaehlt eine andere Figur, wird ein anderes Feld benutzt
    FieldDef nick;
    nick.name = "spitzname";
    nick.type = FieldType::Text;
    p.addOwnField(alice, nick);
    alice.values["spitzname"] = "die Ritterin";
    r.check(renderManuscript(p, "Er sah @Alice.spitzname kommen.") == "Er sah die Ritterin kommen.",
            "manuscript: field reference for another point of view");
    alice.values["spitzname"] = "";
    r.check(renderManuscript(p, "Er sah @Alice.spitzname kommen.") == "Er sah Alice kommen.",
            "manuscript: empty field falls back to the name");

    std::vector<std::string> mentioned = mentionedElements(p, text);
    r.check(mentioned.size() == 1 && mentioned[0] == alice.id, "manuscript: mentions collected once");

    const size_t offset = text.find("Jetzt ist");
    r.check(timeAtOffset(p, text, offset) == 11 * kMinutesPerDay + 9 * kMinutesPerHour,
            "manuscript: time at a position");

    size_t begin = 0, end = 0;
    paragraphAt(text, offset + 3, &begin, &end);
    r.check(text.substr(begin, end - begin).find("Jetzt ist") != std::string::npos,
            "manuscript: paragraph found");
    r.check(countWords("eins zwei  drei\nvier") == 4, "manuscript: word count");

    // unbekannter Name bleibt stehen, statt zu verschwinden
    const std::string unknown = "@Niemand geht.";
    r.check(renderManuscript(p, unknown) == unknown, "manuscript: unknown reference kept");
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

    p.addConnection(p.ensureConnectionType("married_to").id, {alice, bob}).description =
        "Alice und Bob sind verheiratet.";
    p.addConnection(p.ensureConnectionType("owns").id, {alice, sword}).description = "Seit Tag 5.";
    p.addConnection(p.ensureConnectionType("guards").id, {guard, castle}).description =
        "Wache am Haupttor.";

    // Beispiel fuer eine zeitliche Verbindung mit Band in der Timeline
    ConnectionType& atPlace = p.addConnectionType("ist an Ort");
    atPlace.temporal = true;
    atPlace.exclusive = true;
    atPlace.roles[0].name = "Wer";
    atPlace.roles[0].allowedGroups.push_back(p.findGroupByPath("Characters")->id);
    atPlace.roles[1].name = "Wo";
    atPlace.roles[1].allowedGroups.push_back(p.findGroupByPath("Locations")->id);
    auto place = [&](const std::string& who, const std::string& where, long long day) {
        Connection& c = p.addConnection(atPlace.id, {who, where});
        c.hasStart = true;
        c.startTime = day * kMinutesPerDay;
        p.applyExclusivity(c);
    };
    place(alice, castle, 0);
    place(alice, forest, 4);
    place(alice, castle, 10);
    place(bob, castle, 0);

    p.manuscript =
        "## Kapitel 1 - Rueckkehr\n"
        "#Tag 1, 09:00\n"
        "@Alice kam zurueck, und die Stadt war kleiner, als sie sie in Erinnerung hatte.\n"
        "Am Markt stand @Bob, der sie zuerst nicht erkannte.\n\n"
        "\"Du bist es wirklich\", sagte er. @Alice war inzwischen @Alice.age Jahre alt und\n"
        "hatte die Ruhe von jemandem, der lange unterwegs war.\n\n"
        "## Kapitel 2 - Der Fund\n"
        "#Tag 5, 14:00\n"
        "In der Ruine lag das @Sword, halb im Staub. Es glomm schwach, als @Alice es aufhob.\n\n"
        "#Tag 11, 08:00\n"
        "Ein Jahr aelter, @Alice.age nun, stand sie wieder vor der @Castle.\n";

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
    testLegacyConnections(r);
    testManuscript(r);
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
