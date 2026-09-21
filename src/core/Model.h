// Core data model of the story editor.
// Everything the user creates lives here; the vault on disk is a projection of it.
#pragma once

#include <map>
#include <string>
#include <vector>

#include "imgui.h"

namespace se {

// ---------------------------------------------------------------- field types
enum class FieldType {
    Text,
    Integer,
    Float,
    Date,
    Enum,
    Boolean,
    List,
    Reference,
    File,
};

const char* fieldTypeName(FieldType t);          // "Text", "Integer", ...
const char* fieldTypeLabel(FieldType t);         // user facing, German
FieldType   fieldTypeFromName(const std::string& s);
const char* const* fieldTypeLabels(int* count);  // for combo boxes

// A single field of a group template (see spec 3.2.4).
struct FieldDef {
    std::string name;
    FieldType type = FieldType::Text;
    bool required = false;
    std::string defaultValue;
    std::string description;    // tooltip / help text
    std::string displayFormat;  // optional, e.g. "DD.MM.YYYY"
    std::vector<std::string> enumOptions;
};

// ---------------------------------------------------------------------- group
struct Group {
    std::string id;
    std::string name;
    std::string parentId;             // empty => top level group
    ImVec4 color = ImVec4(0, 0, 0, 0);
    bool colorExplicit = false;       // false => derived from parent / auto assigned
    std::vector<FieldDef> fields;     // own template fields; children inherit them
    bool expanded = true;             // UI state, persisted in metadata.json
};

// -------------------------------------------------------------------- element
struct Element {
    std::string id;
    std::string name;
    std::string groupId;
    // field name -> serialised value. Lists are stored as JSON arrays, booleans
    // as "true"/"false", references as "@Group/Path/Name".
    std::map<std::string, std::string> values;
    std::vector<std::string> fieldOrder;  // includes template + user added fields
    // Felder, die es nur bei diesem einen Element gibt (Typ, Default, Optionen).
    // Fields that exist on this single element only.
    std::vector<FieldDef> ownFields;
    std::string filePath;                 // vault relative, e.g. "Characters/Main/Alice.md"
    std::string body;                     // free text below the field block
};

// --------------------------------------------------------------------- action
struct Mutation {
    std::string elementId;
    std::string field;
    std::string oldValue;
    std::string newValue;
};

struct Action {
    std::string id;
    std::string title;
    std::string description;
    std::string type = "Action";  // Dialogue / Action / Scene Change / ...
    long long time = 0;           // absolute story minutes
    bool useRelative = false;
    std::string relativeToId;     // other action id
    long long relativeOffset = 0; // minutes after that action
    std::vector<std::string> elementIds;
    std::vector<std::string> tags;
    std::vector<std::string> attachments;  // vault relative paths
    std::vector<Mutation> mutations;
    bool major = false;
    std::string storyline;
};

// ----------------------------------------------------------------- connection
// Eine Rolle ist ein Platz in einer Verbindung: "Person", "Ort", "Besitzer".
// Welche Gruppen dort eingesetzt werden duerfen, bestimmt der Benutzer.
// A role is one slot of a connection; the user decides which groups may fill it.
struct ConnectionRole {
    std::string name;
    std::vector<std::string> allowedGroups;  // Gruppen-IDs, leer = alle erlaubt
};

// Vorlage fuer Verbindungen - das Gegenstueck zum Gruppen-Template.
struct ConnectionType {
    std::string id;
    std::string name = "Verbindung";
    std::string description;
    std::vector<ConnectionRole> roles;  // mindestens zwei
    bool temporal = false;   // ueber die Timeline setzbar und zeitlich begrenzt
    bool exclusive = true;   // pro Element nur eine gleichzeitig (nur bei temporal)
    int bandRole = 0;        // in wessen Spur das Band laeuft
    int labelRole = 1;       // womit das Band beschriftet wird
    bool showBand = true;    // Band in der Timeline zeichnen
    ImVec4 color = ImVec4(0, 0, 0, 0);
    bool colorExplicit = false;
};

struct Connection {
    std::string id;
    std::string typeId;                  // Verweis auf den ConnectionType
    std::vector<std::string> members;    // ein Element je Rolle
    std::string description;
    // Zeitraum, nur bei zeitlichen Typen benutzt. "bis" offen = gilt weiter.
    bool hasStart = false;
    long long startTime = 0;
    bool hasEnd = false;
    long long endTime = 0;
    std::string blockId;  // optional block membership

    bool has(const std::string& elementId) const;
};

struct Block {
    std::string id;
    std::string name;
    std::string description;
    bool collapsed = false;
};

// ------------------------------------------------------------------- helpers
std::string newId(const char* prefix);
std::string joinList(const std::vector<std::string>& v, const char* sep = ", ");
std::vector<std::string> splitString(const std::string& s, char sep);
std::string trim(const std::string& s);
bool iequalsContains(const std::string& haystack, const std::string& needle);

// List values are stored as a JSON array string; these two convert.
std::vector<std::string> listFromValue(const std::string& value);
std::string listToValue(const std::vector<std::string>& items);

std::string defaultValueFor(const FieldDef& f);

// Saettigung/Helligkeit der automatisch vergebenen Gruppenfarben. Wird vom
// Theme gesetzt, damit die Farben auf hellem und dunklem Grund sitzen.
void setGroupPalette(float saturation, float value);
void groupPalette(float* saturation, float* value);

// Sprache der vom Core erzeugten Texte (Zeitangaben, Markdown-Abschnitte).
// Language of the texts produced by the core (time stamps, markdown sections).
void setEnglishTexts(bool english);
bool englishTexts();

}  // namespace se
