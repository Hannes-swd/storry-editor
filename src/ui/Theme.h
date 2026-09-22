// Every colour used by the editor lives in this scheme - nothing is hardcoded
// at the call site (spec 2.3 / 7.1).
#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "imgui.h"

#include "ui/Lang.h"

namespace se {

struct ColorScheme {
    ImVec4 textPrimary;
    ImVec4 textSecondary;
    ImVec4 backgroundColor;
    ImVec4 panelBackground;
    ImVec4 accentColor;
    ImVec4 warningColor;
    ImVec4 successColor;
    ImVec4 errorColor;

    ImVec4 timelineBackground;
    ImVec4 timelineGrid;
    ImVec4 timelineTrackAlt;
    ImVec4 timelineRuler;
    ImVec4 selectionColor;
    ImVec4 hoverColor;
    ImVec4 ghostColor;
    ImVec4 edgeColor;
    ImVec4 nodeOutline;
    ImVec4 blockColor;

    // Manuskript: das Blatt und die Arbeitsflaeche drumherum
    ImVec4 pageColor;
    ImVec4 pageTextColor;
    ImVec4 workspaceColor;

    // Auswahl fuer Schriftfarbe und Texthervorhebung im Menueband. Das sind
    // Farben *im Dokument* - sie wandern als #RRGGBB in den Text.
    std::vector<ImVec4> textPalette;
    std::vector<ImVec4> highlightPalette;

    std::map<std::string, ImVec4> groupColors;  // optional per group overrides
};

// Zwei mitgelieferte Farbwelten: hell (Standard) und ein neutrales Dunkel -
// beide ohne Blaustich, die Akzentfarbe ist ein Grauton.
enum class ThemePreset { Light, Dark };

struct AppSettings {
    ColorScheme colors;
    ThemePreset preset = ThemePreset::Light;
    Language language = Language::German;
    float fontSize = 17.0f;
    float timelineTrackHeight = 30.0f;
    float timelineHeaderWidth = 210.0f;
    float timelineMinGapPx = 26.0f;
    float timelineMaxGapPx = 260.0f;
    bool timelineCompressGaps = true;
    bool autosave = true;

    // Manuskript: Seite und Grundschrift (auch fuer den Word-Export)
    std::string docFont = "Georgia";
    float docFontSize = 12.0f;     // Punkt
    float docLineSpacing = 1.15f;
    float docMarginCm = 2.5f;
    int docPageFormat = 0;         // 0 = A4, 1 = A5, 2 = Letter
    bool docPageView = true;       // Seiten statt Endlosrolle
    float docZoom = 1.0f;
    bool docShowRuler = true;
    bool docShowOutline = true;
    bool docShowMarks = true;
    bool docRibbonCollapsed = false;
    int docRibbonTab = 1;          // zuletzt gewaehlte Registerkarte (1 = Start)
    std::string lastVault;
    std::string lastProjectName;

    // window visibility
    bool showManuscript = true;
    bool showTimeline = true;
    bool showGroups = true;
    bool showActions = true;
    bool showStory = false;
    bool showConnections = false;
    bool showFiles = false;
    bool showDetails = true;
    bool showSettings = false;
    bool showDemo = false;
};

namespace theme {

AppSettings& settings();
ColorScheme& colors();

void resetToDefault();          // = applyPreset(ThemePreset::Light)
void applyPreset(ThemePreset preset);
const char* presetName(ThemePreset preset);
void applyImGuiStyle();
bool load();
bool save();

// Schriftschnitte fuer das Manuskript. Fehlt einer auf dem System, liefert der
// Zugriff die normale Schrift zurueck - die UI bleibt also immer lesbar.
struct Fonts {
    ImFont* regular = nullptr;
    ImFont* bold = nullptr;
    ImFont* italic = nullptr;
    ImFont* boldItalic = nullptr;
};

Fonts& fonts();
ImFont* fontFor(bool bold, bool italic);

// Schriftarten fuer das Manuskript. Eine Familie wird erst geladen, wenn sie
// zum ersten Mal gebraucht wird; bis dahin liefert familyFont() die
// Oberflaechenschrift. Das Nachladen passiert zwischen zwei Frames.
const std::vector<std::string>& fontFamilies();  // nur die, die installiert sind
ImFont* familyFont(const std::string& family, bool bold, bool italic);
void loadPendingFonts();  // vor ImGui::NewFrame() aufrufen
// Zaehlt jedes Nachladen - wer Textbreiten zwischenspeichert, setzt dann neu.
uint64_t fontGeneration();

// Seitenmasse des gewaehlten Formats in Zentimetern.
void pageSizeCm(int format, float* width, float* height);
const char* pageFormatName(int format);

ImU32 u32(const ImVec4& c, float alphaScale = 1.0f);
ImVec4 mix(const ImVec4& a, const ImVec4& b, float t);
// Fuellfarbe fuer hervorgehobene Schaltflaechen (aktive Filter o.ae.), die in
// beiden Themes genug Kontrast zur Textfarbe behaelt.
ImVec4 accentFill();
bool isLightTheme();
ImVec4 lighten(const ImVec4& c, float amount);
ImVec4 darken(const ImVec4& c, float amount);
ImVec4 withAlpha(const ImVec4& c, float a);
bool isBright(const ImVec4& c);

}  // namespace theme
}  // namespace se
