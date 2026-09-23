// Das Schreibfeld des Manuskripts - ein eigenes Text-Widget statt
// ImGui::InputTextMultiline, damit der Text so aussieht, wie er gemeint ist:
// echte Ueberschriften, Fett, Farben, Ausrichtung, Aufzaehlungen, Seiten.
//
// Der Quelltext bleibt das Manuskript mit seinen Marken (core/Manuscript.h).
// Die Ansicht blendet die Formatzeichen aus; jede Aenderung laeuft ueber ein
// Zeilenmodell (sichtbare Einheiten + Format) und wird danach wieder als
// Quelltext geschrieben. So kann eine Formatierung nie "halb" im Text landen.
//
// The manuscript editor widget. The source stays the marker text; the view
// hides the markup and every edit goes through a per-line unit model that is
// serialised back, so formatting never ends up half-applied.
#pragma once

#include <cstdint>
#include <deque>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "imgui.h"

#include "core/Manuscript.h"

namespace se {

class Editor;
class DocumentView;

// Wie die Seite aussieht - kommt jeden Frame aus den Einstellungen.
struct DocOptions {
    bool readMode = false;   // Werte eingesetzt, nichts editierbar
    bool showMarks = true;   // Zeit-/Aktionsmarken auch im Lesemodus
    bool pageView = true;    // Seiten statt Endlosrolle
    float zoom = 1.0f;
    float pageWidthCm = 21.0f;
    float pageHeightCm = 29.7f;
    float marginCm = 2.5f;        // oben und unten
    float marginLeftCm = 2.5f;
    float marginRightCm = 2.5f;
    std::string font = "Georgia";
    float fontPt = 12.0f;
    float lineSpacing = 1.15f;
    bool spellCheck = false;   // rote Wellen unter unbekannten Woertern
    bool autoCorrect = false;  // AutoKorrektur beim Tippen
    std::string language = "de-DE";
};

struct DocHighlight {
    size_t begin = 0;
    size_t end = 0;
    ImU32 color = 0;
};

// Was im Schreibfeld angeklickt wurde - das Fenster entscheidet, was passiert.
struct DocEvent {
    enum class Kind { None, Element, Action, Note, ContextMenu };
    Kind kind = Kind::None;
    std::string id;     // Element- bzw. Aktions-ID
    size_t pos = 0;     // Anfang der Marke im Quelltext
    size_t end = 0;
    std::string text;   // Kommentartext
    // Rechtsklick auf ein rot markiertes Wort
    size_t spellBegin = 0, spellEnd = 0;
    std::string spellWord;
};

// --------------------------------------------------------------- Zeilenmodell
struct UnitInfo {
    size_t pos = 0;   // Byte-Offset im alten Quelltext
    int token = -1;   // Index in der Tokenliste
    int group = -1;   // gehoert zu einer Element-/Wertmarke (Zeichen fuer Zeichen)
    bool atom = false;  // Aktion, Lesezeichen, Kommentar: nur als Ganzes
};

struct LineModel {
    LineKind kind = LineKind::Body;
    int level = 0;
    LineAlign align = LineAlign::Left;
    ParagraphFormat format;  // Einzuege und Tabstopps (align steht oben)
    std::string prefix;      // "- ", "1. " ... wie er im Text stand
    std::string atomicRaw;   // Zeit- und Trennzeilen bleiben unveraendert
    std::vector<StyledUnit> units;
    std::vector<UnitInfo> info;

    bool atomic() const { return kind == LineKind::Time || kind == LineKind::Break; }
};

// ------------------------------------------------------------------ Dokument
// Gemeinsamer Zustand fuer alle Ansichten (geteiltes Fenster): Text, Parse-
// Ergebnis und die Undo-Geschichte des Schreibfelds.
class ManuscriptDoc {
public:
    // Einmal pro Frame: merkt Aenderungen von aussen (Undo des Projekts,
    // Neu laden, "Aktion aus Absatz") und verwirft dann die eigene Geschichte.
    void sync(Editor& ed);

    Editor& editor() { return *ed_; }
    const Project& project() const;
    const std::string& text() const;
    const std::vector<ManuscriptToken>& tokens();
    const std::vector<ManuscriptLine>& lines();
    uint64_t generation() const { return generation_; }
    uint64_t projectSignature() const { return projectSig_; }

    LineModel model(size_t lineIndex);

    // Ersetzt einen Bereich des Quelltexts und merkt sich das fuer Undo.
    // `typing` fasst schnell aufeinanderfolgendes Tippen zu einem Schritt.
    void replace(size_t pos, size_t len, const std::string& insert, DocumentView* by,
                 size_t cursorAfter, size_t anchorAfter, bool typing = false);

    bool canUndo() const { return !undo_.empty(); }
    bool canRedo() const { return !redo_.empty(); }
    bool undo(DocumentView* view);
    bool redo(DocumentView* view);

    std::vector<DocumentView*> views;

private:
    struct Edit {
        size_t pos = 0;
        std::string removed;
        std::string inserted;
    };
    struct Step {
        std::vector<Edit> edits;
        size_t cursorBefore = 0, anchorBefore = 0;
        size_t cursorAfter = 0, anchorAfter = 0;
        double time = 0.0;
        bool typing = false;
    };

    void touched();
    void shiftOtherViews(DocumentView* by, size_t pos, size_t removed, size_t inserted);

    Editor* ed_ = nullptr;
    std::string lastText_;
    uint64_t generation_ = 1;
    uint64_t parsedGeneration_ = 0;
    uint64_t projectSig_ = 0;
    std::vector<ManuscriptToken> tokens_;
    std::vector<ManuscriptLine> lines_;
    std::deque<Step> undo_;
    std::deque<Step> redo_;
};

// ------------------------------------------------------------------- Ansicht
class DocumentView {
public:
    explicit DocumentView(const char* id) : id_(id) {}

    // Cursor und Auswahl als Byte-Offsets im Quelltext.
    size_t cursor = 0;
    size_t anchor = 0;
    // Format fuer den naechsten getippten Text (Strg+B ohne Auswahl).
    bool pendingActive = false;
    TextStyle pending;

    // Bei geteilter Ansicht empfaengt nur die zuletzt angeklickte Ansicht Tasten.
    bool activeView = true;
    bool clickedThisFrame = false;
    float zoomRequest = 0.0f;  // Strg+Mausrad: gewuenschte Aenderung

    bool hasSelection() const { return cursor != anchor; }
    size_t selBegin() const { return cursor < anchor ? cursor : anchor; }
    size_t selEnd() const { return cursor < anchor ? anchor : cursor; }
    void setCaret(size_t pos, bool keepAnchor = false);
    void select(size_t begin, size_t end);  // Cursor ans Ende, Anker an den Anfang
    void requestFocus() { wantFocus_ = true; }
    void requestScrollToCaret() { scrollToCaret_ = true; }

    // Zeichnet die Ansicht (ohne Lineal) in einen Bereich der Groesse `size`.
    // `claimKeys`: Vorschlagsliste offen - Pfeile/Enter/Tab/Esc gehoeren dem Fenster.
    DocEvent draw(ManuscriptDoc& doc, const DocOptions& options, ImVec2 size,
                  const std::vector<DocHighlight>& highlights, bool claimKeys);

    // Lineal ueber der Ansicht: Zentimeter, Raender, Einzuege und Tabstopps
    // des Absatzes am Cursor - alles mit der Maus verschiebbar. Belegt
    // `height` Pixel an der aktuellen Stelle.
    void drawRuler(ManuscriptDoc& doc, const DocOptions& options, float height);

    // Zustand nach dem letzten draw()
    bool focused() const { return focused_; }
    bool caretVisible() const { return caretValid_; }
    ImVec2 caretScreenPos() const { return caretScreen_; }
    float caretHeight() const { return caretHeight_; }
    int pageCount() const { return pageCount_; }
    int caretPage() const { return caretPage_; }
    float lastViewWidth() const { return viewWidth_; }
    // Rot markierte Stellen im sichtbaren Bereich (Quelltext-Bereiche) - fuer den Selbsttest.
    std::vector<std::pair<size_t, size_t>> visibleSpellIssues() const;

    // Zoom so, dass die Seite in die Breite passt (fuer "Seitenbreite").
    float zoomForPageWidth(const DocOptions& options) const;
    float zoomForWholePage(const DocOptions& options) const;

private:
    struct LineLayout;
    struct Stop {
        size_t pos = 0;
        float x = 0.0f;
        int row = 0;
        uint8_t cls = 0;  // was folgt: 0 Zeilenende, 1 Leerraum, 2 Wort, 3 Sonstiges
    };
    enum class FragKind : uint8_t { Text, Atom, Label, Bullet, Rule };
    struct Frag {
        FragKind kind = FragKind::Text;
        size_t begin = 0, end = 0;  // Quelltext (Text) bzw. Marke (Atom)
        float x = 0.0f, w = 0.0f;
        float rise = 0.0f;           // hoch-/tiefgestellt
        ImFont* font = nullptr;
        float px = 0.0f;
        int token = -1;
        int label = -1;              // im Zeilen-Cache: Index in dessen Beschriftungen
        int styleIndex = -1;         // im Zeilen-Cache: Index in dessen Formaten
        uint8_t atomKind = 0;        // 0 Aktion, 1 Lesezeichen, 2 Kommentar, 3 Element (Lesemodus)
        bool chip = false;           // Element-/Wertmarke im Schreibmodus
        const TextStyle* style = nullptr;  // beim Platzieren gesetzt
        const std::string* text = nullptr;
    };
    struct Row {
        float y = 0.0f, h = 0.0f, baseline = 0.0f;  // Dokumentkoordinaten
        float left = 0.0f, right = 0.0f;            // Inhaltsbereich der Zeile
        int page = 0;
        int line = -1;
        LineLayout* layout = nullptr;  // Textzeile aus dem Zeilen-Cache
        size_t lineBegin = 0;
        size_t firstStop = 0, stopCount = 0;
        size_t firstFrag = 0, fragCount = 0;
    };

    // Eine gesetzte Textzeile mit relativen Positionen - bleibt gueltig,
    // solange sich die Zeile und die Seiteneinstellungen nicht aendern.
    struct LinePart {
        float h = 0.0f, baseline = 0.0f;
        uint32_t firstStop = 0, stopCount = 0;
        uint32_t firstFrag = 0, fragCount = 0;
    };
    struct LineLayout {
        std::vector<LinePart> parts;
        std::vector<Stop> stops;
        std::vector<Frag> frags;
        std::vector<TextStyle> styles;
        std::deque<std::string> labels;
        float spaceBefore = 0.0f, spaceAfter = 0.0f;
        uint64_t lastUsed = 0;
        // Rechtschreibung: sichtbarer Text der Zeile und woher jedes Byte kommt
        std::string plain;
        std::vector<uint32_t> plainPos;            // relativ zum Zeilenanfang
        std::vector<std::pair<uint32_t, uint32_t>> spellIssues;  // relativ
        uint64_t spellGeneration = 0;
    };

    void layout(ManuscriptDoc& doc, const DocOptions& options, float viewWidth);
    void buildLine(ManuscriptDoc& doc, const DocOptions& options, size_t lineIndex, float contentWidth,
                   LineLayout& out);
    size_t stopIndex(size_t pos) const;
    float stopX(size_t pos, int* row) const;
    size_t hitTest(ImVec2 docPos) const;
    void handleKeys(ManuscriptDoc& doc, const DocOptions& options, bool claimKeys);
    void moveCaret(size_t pos, bool extend);
    void ensureCaretVisible(float viewHeight);

    std::string id_;
    bool wantFocus_ = false;
    bool scrollToCaret_ = false;
    bool focused_ = false;
    bool dragging_ = false;
    float preferredX_ = -1.0f;
    double blinkStart_ = 0.0;

    // Layout-Cache
    uint64_t layoutGeneration_ = 0;
    uint64_t layoutSig_ = 0;
    float layoutWidth_ = -1.0f;
    double layoutTime_ = 0.0;
    std::vector<Stop> stops_;
    std::vector<Frag> frags_;
    std::vector<Row> rows_;
    std::deque<std::string> labels_;  // Zeit- und Trennzeilen (Zeiger bleiben gueltig)
    std::unordered_map<uint64_t, LineLayout> lineCache_;
    uint64_t layoutPass_ = 0;
    std::vector<size_t> lineFirstStop_;  // pro Quellzeile, SIZE_MAX = keine
    std::vector<size_t> lineBegins_;
    float docWidth_ = 0.0f, docHeight_ = 0.0f;
    float pageLeft_ = 0.0f, pageWidthPx_ = 0.0f, pageHeightPx_ = 0.0f, marginPx_ = 0.0f;
    float marginLeftPx_ = 0.0f, marginRightPx_ = 0.0f;
    float pageGap_ = 0.0f;
    int pageCount_ = 1;

    // Ergebnis des letzten Frames
    ImVec2 origin_ = ImVec2(0, 0);   // Bildschirmposition von Dokument (0,0)
    float viewWidth_ = 0.0f;
    float viewHeight_ = 0.0f;
    float scrollX_ = 0.0f;
    bool caretValid_ = false;
    ImVec2 caretScreen_ = ImVec2(0, 0);
    float caretHeight_ = 0.0f;
    int caretPage_ = 0;
    ImVec2 rulerOrigin_ = ImVec2(0, 0);
    ImVec2 viewMin_ = ImVec2(0, 0), viewMax_ = ImVec2(0, 0);  // Schreibfeld auf dem Bildschirm
    enum class RulerDrag { None, MarginLeft, MarginRight, First, Hanging, LeftBoth, Right, Tab };
    RulerDrag rulerDrag_ = RulerDrag::None;
    float rulerTabFrom_ = 0.0f;  // welcher Tabstopp gezogen wird
};

// --------------------------------------------------------------- Bearbeiten
// Alles, was das Menueband und die Tastatur am Text veraendern. Jede Funktion
// arbeitet auf der Auswahl bzw. dem Cursor der Ansicht.
namespace docops {

// Tippen: erbt das Format links vom Cursor (oder das vorgemerkte).
void typeText(ManuscriptDoc& doc, DocumentView& view, const std::string& text, bool typing = true);
// Marke oder Quelltext einsetzen (Element, Lesezeichen, Einfuegen aus der Zwischenablage).
void insertSource(ManuscriptDoc& doc, DocumentView& view, const std::string& source);
// Verweis wie "@Alice" einsetzen - folgt direkt ein Buchstabe, kommt ein
// Leerzeichen dazwischen, sonst verschmoelze der Name mit dem Wort.
void insertReference(ManuscriptDoc& doc, DocumentView& view, const std::string& reference);
// Eine eigene Zeile einschieben (Zeitmarke, Szenenwechsel): steht der Cursor
// am Zeilenanfang davor, sonst hinter der aktuellen Zeile.
void insertOwnLine(ManuscriptDoc& doc, DocumentView& view, const std::string& line);
void newParagraph(ManuscriptDoc& doc, DocumentView& view);
void backspace(ManuscriptDoc& doc, DocumentView& view, bool word);
void deleteForward(ManuscriptDoc& doc, DocumentView& view, bool word);
void deleteSelection(ManuscriptDoc& doc, DocumentView& view);

// Zeichenformat der Auswahl aendern - ohne Auswahl wird es vorgemerkt.
void applyStyle(ManuscriptDoc& doc, DocumentView& view,
                const std::function<void(TextStyle&)>& change);
// Gilt `test` fuer die ganze Auswahl (bzw. am Cursor)?
bool selectionHas(ManuscriptDoc& doc, DocumentView& view,
                  const std::function<bool(const TextStyle&)>& test);
TextStyle caretStyle(ManuscriptDoc& doc, DocumentView& view);

// Absatzformat der betroffenen Zeilen
void setLineKind(ManuscriptDoc& doc, DocumentView& view, LineKind kind, int level);
void setAlign(ManuscriptDoc& doc, DocumentView& view, LineAlign align);
// Einzuege/Tabstopps aller betroffenen Absaetze aendern (ein Undo-Schritt).
void setParagraphFormat(ManuscriptDoc& doc, DocumentView& view,
                        const std::function<void(ParagraphFormat&)>& change);
const ManuscriptLine* caretLine(ManuscriptDoc& doc, DocumentView& view);

// Zwischenablage
void copy(ManuscriptDoc& doc, DocumentView& view);
void cut(ManuscriptDoc& doc, DocumentView& view);
void paste(ManuscriptDoc& doc, DocumentView& view);
std::string selectedPlainText(ManuscriptDoc& doc, DocumentView& view);

// Naechstes unbekanntes Wort ab `from` (mit Umlauf zum Anfang).
bool findSpellingError(ManuscriptDoc& doc, size_t from, size_t* begin, size_t* end);

// Ersetzt einen Quelltextbereich (Suchen/Ersetzen) - mit Undo.
void replaceRaw(ManuscriptDoc& doc, DocumentView& view, size_t begin, size_t end,
                const std::string& text);

}  // namespace docops
}  // namespace se
