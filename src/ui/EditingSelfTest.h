// Selbsttest fuer das Schreibfeld des Manuskripts (ohne Fenster).
#pragma once

#include <string>

namespace se {

// Haengt die Ergebnisse an den Bericht an. 0 = alles bestanden.
int runEditingSelfTest(const std::string& reportPath);

}  // namespace se
