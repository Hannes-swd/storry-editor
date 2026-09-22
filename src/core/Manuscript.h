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
//   # Titel / ## Kapitel / ### Szene -> Ueberschriften fuer die Gliederung
//   **fett** / *kursiv* -> Hervorhebung im Text
//   ~~durchgestrichen~~, <u>unterstrichen</u>, <sup>hoch</sup>, <sub>tief</sub>
//   <span style="color:#C00000;background:#FFFF00;font-size:14pt;font-family:Georgia">
//                       -> Farbe, Hintergrund, Groesse, Schriftart
//   - Punkt / 1. Punkt  -> Aufzaehlung bzw. nummerierte Liste
//   ...%%center%%       -> Ausrichtung der Zeile (center, right, justify)
//   %%bm:Name%%         -> Lesezeichen
//   %%note:Text%%       -> Kommentar an dieser Stelle
//   ---                 -> Szenenwechsel (im Export eine zentrierte Trennung)
//
// Alle Formate sind so gewaehlt, dass Obsidian sie ebenfalls darstellt: HTML
// fuer Farbe und Unterstreichung, %%...%% sind dort unsichtbare Kommentare.
//
// Niemand muss diese Zeichen auswendig koennen: das Manuskript-Fenster setzt
// sie ueber das Menueband. Der Parser hier ist die einzige Stelle, die sie
// kennt - auch der Serializer, der sie beim Formatieren neu schreibt.
#pragma once

#include <string>
#include <vector>

#include "core/Project.h"

namespace se {

// Zeichenformat eines Textstuecks. Leere Farbe / Groesse 0 / leere Schrift
// heisst: Vorgabe des Dokuments.
struct TextStyle {
    bool bold = false;
    bool italic = false;
    bool underline = false;
    bool strike = false;
    bool superscript = false;
    bool subscript = false;
    std::string color;       // "#RRGGBB"
    std::string background;  // "#RRGGBB" - Texthervorhebung
    float size = 0.0f;       // Punkt
    std::string font;        // Schriftfamilie

    bool operator==(const TextStyle& o) const;
    bool operator!=(const TextStyle& o) const { return !(*this == o); }
    bool plain() const { return *this == TextStyle(); }
};

struct ManuscriptToken {
    enum class Kind {
        Text,
        Element,
        Value,
        Time,
        Action,
        Heading,
        Break,
        Markup,    // unsichtbare Formatzeichen (**, <u>, %%center%% ...)
        Bookmark,  // %%bm:Name%%   - field = Name
        Note,      // %%note:Text%% - field = Text
    };

    Kind kind = Kind::Text;
    size_t begin = 0;  // Byte-Bereich im Quelltext
    size_t end = 0;
    std::string raw;       // wie es im Text steht (ohne Formatzeichen)
    std::string targetId;  // aufgeloestes Element bzw. Aktion
    std::string field;     // Value: Feldname, Bookmark: Name, Note: Text
    long long time = 0;    // gueltiger Zeitpunkt an dieser Stelle
    bool resolved = false;
    bool bold = false;    // Hervorhebung, die an dieser Stelle gilt
    bool italic = false;
    TextStyle style;      // vollstaendiges Zeichenformat (bold/italic inklusive)
};

// Zerlegt den Text in Fliesstext und Marken. Der Zeitpunkt wird mitgefuehrt.
std::vector<ManuscriptToken> parseManuscript(const Project& p, const std::string& text);

// ------------------------------------------------------------- Zeilen
// Jede Zeile des Quelltexts ist ein Absatz im Editor. Vorspann ("## ", "- ")
// und die Ausrichtungsmarke am Ende gehoeren zur Zeile, nicht zum Inhalt.
enum class LineKind { Body, Heading, Break, Time, Bullet, Numbered };
enum class LineAlign { Left, Center, Right, Justify };

struct ManuscriptLine {
    size_t begin = 0;         // erstes Byte der Zeile
    size_t end = 0;           // Position des '\n' (bzw. Textende)
    size_t contentBegin = 0;  // hinter "## " / "- " / "1. "
    size_t contentEnd = 0;    // vor der Ausrichtungsmarke
    LineKind kind = LineKind::Body;
    int level = 0;            // Ueberschrift: Anzahl der '#'
    int number = 0;           // nummerierte Liste: laufende Nummer (ab 1)
    LineAlign align = LineAlign::Left;
};

std::vector<ManuscriptLine> manuscriptLines(const std::string& text);

// Zeile, in der `offset` liegt (Index in `lines`).
size_t lineIndexAt(const std::vector<ManuscriptLine>& lines, size_t offset);

// Vorspann fuer eine Zeilenart ("## ", "- ", ...) und Marke fuer eine
// Ausrichtung ("%%center%%", leer fuer links).
std::string linePrefix(LineKind kind, int level);
std::string alignMarker(LineAlign align);

// ---------------------------------------------------------- Serializer
// Ein sichtbares Zeichen (oder eine ganze Marke wie "@Alice") mit Format.
struct StyledUnit {
    std::string raw;
    TextStyle style;
};

// Schreibt eine Zeile aus Einheiten zurueck in Quelltext. `positions` erhaelt
// fuer jede Einheit den Byte-Offset im Ergebnis, dazu am Ende die Laenge des
// sichtbaren Inhalts (Position hinter der letzten Einheit).
std::string serializeUnits(const std::vector<StyledUnit>& units,
                           std::vector<size_t>* positions = nullptr);

// Setzt die Werte ein - fuer Lesemodus und Export. Mit `keepFormatting`
// bleiben die Formatzeichen stehen (Lesefassung im Vault, Obsidian zeigt sie).
std::string renderManuscript(const Project& p, const std::string& text,
                             bool keepFormatting = false);

// Was dieser Text an einem Element haengt: alle erwaehnten Element-IDs.
std::vector<std::string> mentionedElements(const Project& p, const std::string& text);

// Zeitpunkt, der an einer bestimmten Stelle im Text gilt.
long long timeAtOffset(const Project& p, const std::string& text, size_t offset);

// Absatzgrenzen um eine Position herum (fuer "Aktion aus Absatz").
void paragraphAt(const std::string& text, size_t offset, size_t* begin, size_t* end);

size_t countWords(const std::string& text);

// Kommentar- und Lesezeichentext darf die Marke nicht selbst beenden.
std::string sanitizeMarkerText(const std::string& text);

// ------------------------------------------------------------------ Suche
// Alle Fundstellen von `needle` in `haystack` (Gross-/Kleinschreibung egal,
// solange `caseSensitive` aus ist). Gibt die Byte-Offsets der Treffer zurueck.
std::vector<size_t> findAll(const std::string& haystack, const std::string& needle,
                            bool caseSensitive);

}  // namespace se
