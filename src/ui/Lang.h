// Zweisprachige Oberflaeche / bilingual user interface.
//
// Die deutschen Texte sind die Schluessel: TR("Neue Aktion") liefert im
// deutschen Modus genau diesen Text, im englischen die Uebersetzung aus der
// Tabelle in Lang.cpp. Fehlt ein Eintrag, bleibt der deutsche Text stehen -
// die UI bleibt also immer bedienbar.
//
// The German strings are the keys: TR("Neue Aktion") returns that text in
// German mode and the English translation from the table in Lang.cpp
// otherwise. A missing entry falls back to the German text.
#pragma once

#include <string>

namespace se {

enum class Language { German, English };

namespace lang {

Language current();
void set(Language language);
const char* name(Language language);      // "Deutsch" / "English"
Language fromCode(const std::string& code);  // "de" / "en"
const char* code(Language language);

// Uebersetzt einen deutschen UI-Text / translates a German UI string.
const char* tr(const char* german);
std::string trs(const char* german);

// Fenstertitel mit stabiler ImGui-ID: "Aktionen###actions". Dadurch bleibt das
// Docking-Layout beim Sprachwechsel erhalten.
const char* window(const char* german, const char* id);

}  // namespace lang
}  // namespace se

#define TR(text) ::se::lang::tr(text)
#define TRS(text) ::se::lang::trs(text)
#define TWIN(text, id) ::se::lang::window(text, id)
