// Manuskript als Word-Datei (.docx) schreiben.
//
// Exportiert wird der *fertige* Text: alle @-Verweise stehen mit ihrem Wert
// zum jeweiligen Zeitpunkt darin, Zeit- und Aktionsmarken tauchen nicht auf.
// Das Paket wird selbst gebaut (ZIP mit unkomprimierten Eintraegen), damit
// keine zusaetzliche Bibliothek noetig ist.
//
// Writes the manuscript as a Word file: values are baked in, time and action
// markers are gone. The .docx package is built by hand (a ZIP with stored
// entries) so that no extra dependency is needed.
#pragma once

#include <string>

#include "core/Project.h"

namespace se {

// Baut das komplette .docx im Speicher (auch fuer den Selbsttest).
std::string buildManuscriptDocx(const Project& p, const std::string& title);

// Schreibt das Manuskript nach `path`. Gibt bei Fehlern false zurueck.
bool exportManuscriptDocx(const Project& p, const std::string& path, std::string* err);

}  // namespace se
