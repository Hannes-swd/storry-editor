// Bausteine fuer das Menueband des Manuskripts - aufgebaut wie in Word:
// Registerkarten, darunter Gruppen mit kleinen Werkzeugknoepfen (zwei Reihen)
// oder grossen Knoepfen mit Beschriftung, der Gruppenname steht unten.
//
// Die Symbole werden gezeichnet statt aus einer Symbolschrift geladen - so
// gibt es keine zusaetzliche Datei und sie folgen den Farben des Designs.
#pragma once

#include "imgui.h"

namespace se::ribbon {

using IconFn = void (*)(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col);

// Hoehe einer Werkzeugreihe; das Band hat zwei Reihen plus Gruppennamen.
float rowHeight();
float bodyHeight();

// Registerkarten ueber dem Band. Liefert true, wenn gewechselt wurde.
bool tabStrip(int* current, const char* const* labels, int count);

// Beginn des Bandes (merkt sich die Oberkante fuer die Gruppennamen).
void beginBody();
void endBody();

void beginGroup();
void endGroup(const char* label);

// Kleiner Knopf mit Symbol (eine Reihe hoch).
bool tool(const char* id, IconFn icon, const char* tip, bool active = false, bool enabled = true);
// Wie tool(), mit Farbbalken unter dem Symbol (Schriftfarbe, Hervorhebung).
bool colorTool(const char* id, IconFn icon, ImU32 bar, const char* tip);
// Schmaler Pfeil neben einem Knopf, oeffnet ein Menue.
bool dropArrow(const char* id, const char* tip);
// Grosser Knopf ueber zwei Reihen mit Beschriftung.
bool big(const char* id, IconFn icon, const char* label, const char* tip, bool active = false,
         bool dropdown = false);
// Kleiner Knopf mit Symbol und Text (eine Reihe).
bool labeled(const char* id, IconFn icon, const char* label, const char* tip, bool active = false,
             bool enabled = true);

namespace icon {
void bold(ImDrawList*, ImVec2, ImVec2, ImU32);
void italic(ImDrawList*, ImVec2, ImVec2, ImU32);
void underline(ImDrawList*, ImVec2, ImVec2, ImU32);
void strike(ImDrawList*, ImVec2, ImVec2, ImU32);
void subscript(ImDrawList*, ImVec2, ImVec2, ImU32);
void superscript(ImDrawList*, ImVec2, ImVec2, ImU32);
void grow(ImDrawList*, ImVec2, ImVec2, ImU32);
void shrink(ImDrawList*, ImVec2, ImVec2, ImU32);
void clearFormat(ImDrawList*, ImVec2, ImVec2, ImU32);
void fontColor(ImDrawList*, ImVec2, ImVec2, ImU32);
void highlighter(ImDrawList*, ImVec2, ImVec2, ImU32);
void bullets(ImDrawList*, ImVec2, ImVec2, ImU32);
void numbering(ImDrawList*, ImVec2, ImVec2, ImU32);
void alignLeft(ImDrawList*, ImVec2, ImVec2, ImU32);
void alignCenter(ImDrawList*, ImVec2, ImVec2, ImU32);
void alignRight(ImDrawList*, ImVec2, ImVec2, ImU32);
void alignJustify(ImDrawList*, ImVec2, ImVec2, ImU32);
void lineSpacing(ImDrawList*, ImVec2, ImVec2, ImU32);
void undo(ImDrawList*, ImVec2, ImVec2, ImU32);
void redo(ImDrawList*, ImVec2, ImVec2, ImU32);
void save(ImDrawList*, ImVec2, ImVec2, ImU32);
void paste(ImDrawList*, ImVec2, ImVec2, ImU32);
void cut(ImDrawList*, ImVec2, ImVec2, ImU32);
void copy(ImDrawList*, ImVec2, ImVec2, ImU32);
void find(ImDrawList*, ImVec2, ImVec2, ImU32);
void replace(ImDrawList*, ImVec2, ImVec2, ImU32);
void selectAll(ImDrawList*, ImVec2, ImVec2, ImU32);
void element(ImDrawList*, ImVec2, ImVec2, ImU32);
void value(ImDrawList*, ImVec2, ImVec2, ImU32);
void action(ImDrawList*, ImVec2, ImVec2, ImU32);
void paragraphAction(ImDrawList*, ImVec2, ImVec2, ImU32);
void clock(ImDrawList*, ImVec2, ImVec2, ImU32);
void sceneBreak(ImDrawList*, ImVec2, ImVec2, ImU32);
void bookmark(ImDrawList*, ImVec2, ImVec2, ImU32);
void comment(ImDrawList*, ImVec2, ImVec2, ImU32);
void prev(ImDrawList*, ImVec2, ImVec2, ImU32);
void next(ImDrawList*, ImVec2, ImVec2, ImU32);
void trash(ImDrawList*, ImVec2, ImVec2, ImU32);
void outline(ImDrawList*, ImVec2, ImVec2, ImU32);
void ruler(ImDrawList*, ImVec2, ImVec2, ImU32);
void marks(ImDrawList*, ImVec2, ImVec2, ImU32);
void split(ImDrawList*, ImVec2, ImVec2, ImU32);
void focus(ImDrawList*, ImVec2, ImVec2, ImU32);
void readMode(ImDrawList*, ImVec2, ImVec2, ImU32);
void pageView(ImDrawList*, ImVec2, ImVec2, ImU32);
void webView(ImDrawList*, ImVec2, ImVec2, ImU32);
void zoom(ImDrawList*, ImVec2, ImVec2, ImU32);
void onePage(ImDrawList*, ImVec2, ImVec2, ImU32);
void pageWidth(ImDrawList*, ImVec2, ImVec2, ImU32);
void margins(ImDrawList*, ImVec2, ImVec2, ImU32);
void pageSize(ImDrawList*, ImVec2, ImVec2, ImU32);
void word(ImDrawList*, ImVec2, ImVec2, ImU32);
void help(ImDrawList*, ImVec2, ImVec2, ImU32);
void settings(ImDrawList*, ImVec2, ImVec2, ImU32);
void count(ImDrawList*, ImVec2, ImVec2, ImU32);
void chevronUp(ImDrawList*, ImVec2, ImVec2, ImU32);
void chevronDown(ImDrawList*, ImVec2, ImVec2, ImU32);
}  // namespace icon

}  // namespace se::ribbon
