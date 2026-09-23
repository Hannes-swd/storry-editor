// Rechtschreibpruefung ueber die eingebaute Windows-Schnittstelle
// (ISpellChecker, ab Windows 8). Keine Woerterbuchdateien im Programm: es
// gilt, was Windows fuer die installierten Sprachen mitbringt - dieselben
// Woerterbuecher wie in Edge oder den Windows-Einstellungen.
//
// Eigene Woerter ("Zum Woerterbuch hinzufuegen") landen in einer Datei im
// Konfigurationsordner und werden beim Start wieder eingelesen - das
// Windows-Benutzerwoerterbuch bleibt unberuehrt.
//
// Spell checking via the Windows ISpellChecker API. The program ships no
// dictionaries; user additions live in its own file, not in the system's.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace se::spell {

struct Issue {
    size_t begin = 0;  // Byte-Offset im geprueften UTF-8-Text
    size_t end = 0;
    std::string replacement;  // gesetzt, wenn Windows eine AutoKorrektur kennt
};

// Sprache waehlen ("de-DE", "en-US"). Liefert false, wenn Windows sie nicht
// anbietet - dann findet check() nichts.
bool setLanguage(const std::string& tag);
const std::string& language();
bool available();                       // Pruefung fuer die aktuelle Sprache moeglich
bool supported(const std::string& tag); // bietet Windows diese Sprache an?
std::vector<std::string> installedLanguages();  // alle, die Windows anbietet

// Fehler in einem Text (Woerter, die Windows nicht kennt).
std::vector<Issue> check(const std::string& utf8);
// Ist dieses eine Wort bekannt? Leere/unbekannte Sprache: true.
bool isCorrect(const std::string& word);
// Vorschlaege fuer ein falsches Wort (hoechstens `max`).
std::vector<std::string> suggest(const std::string& word, size_t max = 6);
// AutoKorrektur-Ersatz fuer ein Wort, sonst leer.
std::string autoCorrection(const std::string& word);

// Wort dauerhaft als richtig merken (eigene Liste) bzw. nur fuer diese Sitzung.
void addToDictionary(const std::string& word);
void ignoreForSession(const std::string& word);
// Namen, die immer als richtig gelten (Elemente des Projekts).
void setKnownNames(const std::vector<std::string>& names);

// Zaehlt jede Aenderung (Sprache, Woerterbuch) - wer Ergebnisse
// zwischenspeichert, prueft danach neu.
uint64_t generation();

}  // namespace se::spell
