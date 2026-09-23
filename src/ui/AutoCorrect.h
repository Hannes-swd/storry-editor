// AutoKorrektur wie in Word - greift beim Tippen im Manuskript:
//
//  - typografische Anfuehrungszeichen: "..." -> „...“ (Deutsch) bzw. “...” (Englisch),
//    ' -> ’ bzw. ‚ / ‘ am Wortanfang
//  - "..." -> "…", " - " / " -- " -> " – "
//  - Satzanfang gross ("das Haus. es war" -> "Es war"), mit Ausnahmen wie "z.B."
//  - ZWei GRosse Anfangsbuchstaben -> "Zwei", "Grosse"
//  - Tippfehler, fuer die Windows eine feste Korrektur kennt
//
// Jede Korrektur ist ein eigener Undo-Schritt: Strg+Z direkt danach nimmt nur
// die Korrektur zurueck und laesst das Getippte stehen.
#pragma once

#include <string>

namespace se {

class ManuscriptDoc;
class DocumentView;

namespace autocorrect {

// Getippte Zeichen vor dem Einfuegen umwandeln (Anfuehrungszeichen).
std::string transformTyped(const std::string& text, size_t cursor, const std::string& typed,
                           const std::string& language);

// Nach dem Einfuegen: Wort vor dem Trennzeichen korrigieren, "..." und
// Gedankenstriche setzen. `typed` ist, was eben eingefuegt wurde.
void afterTyped(ManuscriptDoc& doc, DocumentView& view, const std::string& typed,
                const std::string& language);

// Vor einem Absatzwechsel (Enter): das letzte Wort der Zeile pruefen.
void beforeParagraph(ManuscriptDoc& doc, DocumentView& view, const std::string& language);

}  // namespace autocorrect
}  // namespace se
