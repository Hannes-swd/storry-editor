// Oberflaechentest per Skript: Mit STORYEDITOR_UITEST=<skript.txt> laeuft das
// Programm ausserhalb des sichtbaren Bildschirms, bekommt Maus und Tastatur
// aus dem Skript direkt in ImGui eingespeist und fotografiert sein eigenes
// Bild aus der Grafikkarte. Echte Maus und Tastatur bleiben unberuehrt.
//
// Befehle (eine Zeile je Befehl, Koordinaten im Fenster, Pfade relativ zum Skript):
//   wait N                  N Frames warten
//   move X Y                Maus bewegen
//   click X Y [right]       klicken (links oder rechts)
//   dblclick X Y            Doppelklick
//   drag X1 Y1 X2 Y2        mit gedrueckter Taste ziehen
//   key [ctrl+][shift+][alt+]NAME   Taste (A-Z, Enter, Backspace, Delete, Tab,
//                                    Escape, Left, Right, Up, Down, Home, End, F7)
//   type TEXT               Text tippen (Rest der Zeile)
//   shot DATEI.png          Bildschirmfoto des Fensters
//   dump DATEI              Manuskript-Quelltext schreiben
//   export DATEI.docx       Word-Export
//   quit                    beenden
//
// UI test by script: runs off-screen, feeds input straight into ImGui and
// captures the back buffer - the real mouse and keyboard are never touched.
#pragma once

#include <string>

struct ID3D11Device;
struct ID3D11DeviceContext;
struct IDXGISwapChain;

namespace se {
class Editor;

namespace uitest {

bool active();  // STORYEDITOR_UITEST gesetzt?
void beforeNewFrame(Editor& ed);
void afterRender(ID3D11Device* device, ID3D11DeviceContext* context, IDXGISwapChain* swapChain);
bool finished();

}  // namespace uitest
}  // namespace se
