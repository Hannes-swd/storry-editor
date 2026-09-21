// Every colour used by the editor lives in this scheme - nothing is hardcoded
// at the call site (spec 2.3 / 7.1).
#pragma once

#include <map>
#include <string>

#include "imgui.h"

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

    std::map<std::string, ImVec4> groupColors;  // optional per group overrides
};

struct AppSettings {
    ColorScheme colors;
    float fontSize = 17.0f;
    float timelineTrackHeight = 30.0f;
    float timelineHeaderWidth = 210.0f;
    float timelineMinGapPx = 26.0f;
    float timelineMaxGapPx = 260.0f;
    bool timelineCompressGaps = true;
    bool autosave = true;
    std::string lastVault;
    std::string lastProjectName;

    // window visibility
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

void resetToDefault();
void applyImGuiStyle();
bool load();
bool save();

ImU32 u32(const ImVec4& c, float alphaScale = 1.0f);
ImVec4 lighten(const ImVec4& c, float amount);
ImVec4 darken(const ImVec4& c, float amount);
ImVec4 withAlpha(const ImVec4& c, float a);
bool isBright(const ImVec4& c);

}  // namespace theme
}  // namespace se
