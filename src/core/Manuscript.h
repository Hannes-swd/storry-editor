// Das Manuskript: ein durchgehender Text, in dem Verweise als kurze Marken
// stehen. Beim Schreiben sind es anklickbare Variablen, beim Lesen/Export
// werden die Werte eingesetzt.
//
// The manuscript: one continuous text with short markers. While writing they
// are clickable variables; when reading or exporting they resolve to values.
//
//   @Alice              -> Verweis auf ein Element (Name wird eingesetzt)
//   @Alice.age          -> Wert des Feldes zu diesem Zeitpunkt der Geschichte
//   @Characters/Main/Alice  -> eindeutiger Pfad, falls Namen doppelt sind
//   #Tag 5, 14:00       -> ab hier gilt dieser Zeitpunkt
//   !act:<id>           -> verknuepfte Aktion (Titel wird eingesetzt)
//   ## Kapitel          -> Ueberschrift fuer die Gliederung
//   **fett** / *kursiv* -> Hervorhebung im Text
//   ---                 -> Szenenwechsel (im Export eine zentrierte Trennung)
//
// Niemand muss diese Zeichen auswendig koennen: das Manuskript-Fenster setzt
// sie ueber die Menues "Einfuegen" und "Format". Der Parser hier ist die
// einzige Stelle, die sie kennt.
#pragma once

#include <string>
#include <vector>

#include "core/Project.h"

namespace se {

struct ManuscriptToken {
    enum class Kind { Text, Element, Value, Time, Action, Heading, Break };

    Kind kind = Kind::Text;
    size_t begin = 0;  // Byte-Bereich im Quelltext
    size_t end = 0;
    std::string raw;       // wie es im Text steht (ohne Formatzeichen)
    std::string targetId;  // aufgeloestes Element bzw. Aktion
    std::string field;     // nur bei Value
    long long time = 0;    // gueltiger Zeitpunkt an dieser Stelle
    bool resolved = false;
    bool bold = false;    // Hervorhebung, die an dieser Stelle gilt
    bool italic = false;
};

// Zerlegt den Text in Fliesstext und Marken. Der Zeitpunkt wird mitgefuehrt.
std::vector<ManuscriptToken> parseManuscript(const Project& p, const std::string& text);

// Setzt alle Werte ein - fuer Lesemodus und Export. Mit `keepFormatting`
// bleiben die Zeichen fuer fett/kursiv/Szenenwechsel stehen, damit der
// Word-Export sie in echte Formatierung uebersetzen kann.
std::string renderManuscript(const Project& p, const std::string& text,
                             bool keepFormatting = false);

// Was dieser Text an einem Element haengt: alle erwaehnten Element-IDs.
std::vector<std::string> mentionedElements(const Project& p, const std::string& text);

// Zeitpunkt, der an einer bestimmten Stelle im Text gilt.
long long timeAtOffset(const Project& p, const std::string& text, size_t offset);

// Absatzgrenzen um eine Position herum (fuer "Aktion aus Absatz").
void paragraphAt(const std::string& text, size_t offset, size_t* begin, size_t* end);

size_t countWords(const std::string& text);

// ------------------------------------------------------------------ Suche
// Alle Fundstellen von `needle` in `haystack` (Gross-/Kleinschreibung egal,
// solange `caseSensitive` aus ist). Gibt die Byte-Offsets der Treffer zurueck.
std::vector<size_t> findAll(const std::string& haystack, const std::string& needle,
                            bool caseSensitive);

}  // namespace se
