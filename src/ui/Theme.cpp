#include "ui/Theme.h"

#include <algorithm>

#include <nlohmann/json.hpp>

#include "app/Platform.h"

using nlohmann::json;

namespace se::theme {
namespace {

AppSettings g_settings;
bool g_initialised = false;

json colorToJson(const ImVec4& c) { return json::array({c.x, c.y, c.z, c.w}); }

ImVec4 colorFromJson(const json& j, const ImVec4& fallback) {
    if (j.is_array() && j.size() >= 4)
        return ImVec4(j[0].get<float>(), j[1].get<float>(), j[2].get<float>(), j[3].get<float>());
    return fallback;
}

std::string configFile() { return platform::appConfigDir() + "/settings.json"; }

}  // namespace

AppSettings& settings() {
    if (!g_initialised) {
        resetToDefault();
        g_initialised = true;
    }
    return g_settings;
}

ColorScheme& colors() { return settings().colors; }

void resetToDefault() {
    g_initialised = true;
    ColorScheme& c = g_settings.colors;
    c.textPrimary = ImVec4(0.92f, 0.93f, 0.95f, 1.00f);
    c.textSecondary = ImVec4(0.62f, 0.65f, 0.70f, 1.00f);
    c.backgroundColor = ImVec4(0.11f, 0.12f, 0.14f, 1.00f);
    c.panelBackground = ImVec4(0.15f, 0.16f, 0.19f, 1.00f);
    c.accentColor = ImVec4(0.29f, 0.56f, 0.89f, 1.00f);  // #4A90E2
    c.warningColor = ImVec4(0.96f, 0.65f, 0.14f, 1.00f);  // #F5A623
    c.successColor = ImVec4(0.49f, 0.83f, 0.13f, 1.00f);  // #7ED321
    c.errorColor = ImVec4(0.82f, 0.01f, 0.11f, 1.00f);    // #D0021B
    c.timelineBackground = ImVec4(0.13f, 0.14f, 0.17f, 1.00f);
    c.timelineGrid = ImVec4(0.26f, 0.28f, 0.33f, 1.00f);
    c.timelineTrackAlt = ImVec4(0.17f, 0.18f, 0.22f, 1.00f);
    c.timelineRuler = ImVec4(0.72f, 0.75f, 0.80f, 1.00f);
    c.selectionColor = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    c.hoverColor = ImVec4(1.00f, 0.95f, 0.70f, 1.00f);
    c.ghostColor = ImVec4(1.00f, 1.00f, 1.00f, 0.35f);
    c.edgeColor = ImVec4(0.58f, 0.62f, 0.70f, 1.00f);
    c.nodeOutline = ImVec4(0.08f, 0.09f, 0.11f, 1.00f);
    c.blockColor = ImVec4(0.45f, 0.40f, 0.75f, 1.00f);
    c.groupColors.clear();
}

ImU32 u32(const ImVec4& c, float alphaScale) {
    ImVec4 col = c;
    col.w *= alphaScale;
    return ImGui::ColorConvertFloat4ToU32(col);
}

ImVec4 lighten(const ImVec4& c, float amount) {
    return ImVec4(std::min(1.0f, c.x + amount), std::min(1.0f, c.y + amount),
                  std::min(1.0f, c.z + amount), c.w);
}

ImVec4 darken(const ImVec4& c, float amount) {
    return ImVec4(std::max(0.0f, c.x - amount), std::max(0.0f, c.y - amount),
                  std::max(0.0f, c.z - amount), c.w);
}

ImVec4 withAlpha(const ImVec4& c, float a) { return ImVec4(c.x, c.y, c.z, a); }

bool isBright(const ImVec4& c) { return (c.x * 0.299f + c.y * 0.587f + c.z * 0.114f) > 0.6f; }

void applyImGuiStyle() {
    const ColorScheme& c = colors();
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding = 4.0f;
    s.FrameRounding = 3.0f;
    s.GrabRounding = 3.0f;
    s.TabRounding = 3.0f;
    s.ScrollbarRounding = 3.0f;
    s.WindowPadding = ImVec2(8, 8);
    s.FramePadding = ImVec2(7, 4);
    s.ItemSpacing = ImVec2(8, 5);
    s.WindowTitleAlign = ImVec2(0.0f, 0.5f);
    s.WindowMenuButtonPosition = ImGuiDir_None;

    ImVec4* col = s.Colors;
    col[ImGuiCol_Text] = c.textPrimary;
    col[ImGuiCol_TextDisabled] = c.textSecondary;
    col[ImGuiCol_WindowBg] = c.backgroundColor;
    col[ImGuiCol_ChildBg] = withAlpha(c.panelBackground, 0.0f);
    col[ImGuiCol_PopupBg] = darken(c.panelBackground, 0.03f);
    col[ImGuiCol_Border] = withAlpha(c.textSecondary, 0.35f);
    col[ImGuiCol_FrameBg] = c.panelBackground;
    col[ImGuiCol_FrameBgHovered] = lighten(c.panelBackground, 0.06f);
    col[ImGuiCol_FrameBgActive] = lighten(c.panelBackground, 0.12f);
    col[ImGuiCol_TitleBg] = darken(c.panelBackground, 0.04f);
    col[ImGuiCol_TitleBgActive] = darken(c.accentColor, 0.25f);
    col[ImGuiCol_TitleBgCollapsed] = darken(c.panelBackground, 0.06f);
    col[ImGuiCol_MenuBarBg] = darken(c.panelBackground, 0.02f);
    col[ImGuiCol_ScrollbarBg] = withAlpha(c.backgroundColor, 0.6f);
    col[ImGuiCol_ScrollbarGrab] = lighten(c.panelBackground, 0.10f);
    col[ImGuiCol_ScrollbarGrabHovered] = lighten(c.panelBackground, 0.16f);
    col[ImGuiCol_ScrollbarGrabActive] = c.accentColor;
    col[ImGuiCol_CheckMark] = c.accentColor;
    col[ImGuiCol_SliderGrab] = c.accentColor;
    col[ImGuiCol_SliderGrabActive] = lighten(c.accentColor, 0.1f);
    col[ImGuiCol_Button] = lighten(c.panelBackground, 0.05f);
    col[ImGuiCol_ButtonHovered] = withAlpha(c.accentColor, 0.75f);
    col[ImGuiCol_ButtonActive] = c.accentColor;
    col[ImGuiCol_Header] = withAlpha(c.accentColor, 0.45f);
    col[ImGuiCol_HeaderHovered] = withAlpha(c.accentColor, 0.65f);
    col[ImGuiCol_HeaderActive] = c.accentColor;
    col[ImGuiCol_Separator] = withAlpha(c.textSecondary, 0.30f);
    col[ImGuiCol_SeparatorHovered] = c.accentColor;
    col[ImGuiCol_SeparatorActive] = lighten(c.accentColor, 0.1f);
    col[ImGuiCol_ResizeGrip] = withAlpha(c.accentColor, 0.35f);
    col[ImGuiCol_ResizeGripHovered] = withAlpha(c.accentColor, 0.65f);
    col[ImGuiCol_ResizeGripActive] = c.accentColor;
    col[ImGuiCol_Tab] = darken(c.panelBackground, 0.02f);
    col[ImGuiCol_TabHovered] = withAlpha(c.accentColor, 0.7f);
    col[ImGuiCol_TabSelected] = darken(c.accentColor, 0.2f);
    col[ImGuiCol_TabDimmed] = darken(c.panelBackground, 0.04f);
    col[ImGuiCol_TabDimmedSelected] = darken(c.accentColor, 0.32f);
    col[ImGuiCol_DockingPreview] = withAlpha(c.accentColor, 0.6f);
    col[ImGuiCol_DockingEmptyBg] = darken(c.backgroundColor, 0.03f);
    col[ImGuiCol_TableHeaderBg] = darken(c.panelBackground, 0.02f);
    col[ImGuiCol_TableBorderStrong] = withAlpha(c.textSecondary, 0.4f);
    col[ImGuiCol_TableBorderLight] = withAlpha(c.textSecondary, 0.22f);
    col[ImGuiCol_TableRowBg] = withAlpha(c.panelBackground, 0.0f);
    col[ImGuiCol_TableRowBgAlt] = withAlpha(c.panelBackground, 0.45f);
    col[ImGuiCol_TextSelectedBg] = withAlpha(c.accentColor, 0.45f);
    col[ImGuiCol_NavCursor] = c.accentColor;
    col[ImGuiCol_ModalWindowDimBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.55f);
}

bool save() {
    AppSettings& s = settings();
    json j;
    j["font_size"] = s.fontSize;
    j["timeline"] = {{"track_height", s.timelineTrackHeight},
                     {"header_width", s.timelineHeaderWidth},
                     {"min_gap", s.timelineMinGapPx},
                     {"max_gap", s.timelineMaxGapPx},
                     {"compress_gaps", s.timelineCompressGaps}};
    j["autosave"] = s.autosave;
    j["last_vault"] = s.lastVault;
    j["last_project_name"] = s.lastProjectName;
    j["windows"] = {{"timeline", s.showTimeline}, {"groups", s.showGroups},
                    {"actions", s.showActions},   {"story", s.showStory},
                    {"connections", s.showConnections}, {"files", s.showFiles},
                    {"details", s.showDetails},   {"settings", s.showSettings}};

    const ColorScheme& c = s.colors;
    json jc;
    jc["text_primary"] = colorToJson(c.textPrimary);
    jc["text_secondary"] = colorToJson(c.textSecondary);
    jc["background"] = colorToJson(c.backgroundColor);
    jc["panel"] = colorToJson(c.panelBackground);
    jc["accent"] = colorToJson(c.accentColor);
    jc["warning"] = colorToJson(c.warningColor);
    jc["success"] = colorToJson(c.successColor);
    jc["error"] = colorToJson(c.errorColor);
    jc["timeline_background"] = colorToJson(c.timelineBackground);
    jc["timeline_grid"] = colorToJson(c.timelineGrid);
    jc["timeline_track_alt"] = colorToJson(c.timelineTrackAlt);
    jc["timeline_ruler"] = colorToJson(c.timelineRuler);
    jc["selection"] = colorToJson(c.selectionColor);
    jc["hover"] = colorToJson(c.hoverColor);
    jc["ghost"] = colorToJson(c.ghostColor);
    jc["edge"] = colorToJson(c.edgeColor);
    jc["node_outline"] = colorToJson(c.nodeOutline);
    jc["block"] = colorToJson(c.blockColor);
    json overrides = json::object();
    for (const auto& kv : c.groupColors) overrides[kv.first] = colorToJson(kv.second);
    jc["group_overrides"] = overrides;
    j["colors"] = jc;

    std::string err;
    return platform::writeFile(configFile(), j.dump(2), &err);
}

bool load() {
    AppSettings& s = settings();
    std::string text;
    if (!platform::readFile(configFile(), &text)) return false;
    json j;
    try {
        j = json::parse(text);
    } catch (...) {
        return false;
    }

    s.fontSize = j.value("font_size", s.fontSize);
    if (j.contains("timeline")) {
        const json& t = j["timeline"];
        s.timelineTrackHeight = t.value("track_height", s.timelineTrackHeight);
        s.timelineHeaderWidth = t.value("header_width", s.timelineHeaderWidth);
        s.timelineMinGapPx = t.value("min_gap", s.timelineMinGapPx);
        s.timelineMaxGapPx = t.value("max_gap", s.timelineMaxGapPx);
        s.timelineCompressGaps = t.value("compress_gaps", s.timelineCompressGaps);
    }
    s.autosave = j.value("autosave", s.autosave);
    s.lastVault = j.value("last_vault", s.lastVault);
    s.lastProjectName = j.value("last_project_name", s.lastProjectName);
    if (j.contains("windows")) {
        const json& w = j["windows"];
        s.showTimeline = w.value("timeline", s.showTimeline);
        s.showGroups = w.value("groups", s.showGroups);
        s.showActions = w.value("actions", s.showActions);
        s.showStory = w.value("story", s.showStory);
        s.showConnections = w.value("connections", s.showConnections);
        s.showFiles = w.value("files", s.showFiles);
        s.showDetails = w.value("details", s.showDetails);
        s.showSettings = w.value("settings", s.showSettings);
    }
    if (j.contains("colors")) {
        const json& jc = j["colors"];
        ColorScheme& c = s.colors;
        c.textPrimary = colorFromJson(jc.value("text_primary", json()), c.textPrimary);
        c.textSecondary = colorFromJson(jc.value("text_secondary", json()), c.textSecondary);
        c.backgroundColor = colorFromJson(jc.value("background", json()), c.backgroundColor);
        c.panelBackground = colorFromJson(jc.value("panel", json()), c.panelBackground);
        c.accentColor = colorFromJson(jc.value("accent", json()), c.accentColor);
        c.warningColor = colorFromJson(jc.value("warning", json()), c.warningColor);
        c.successColor = colorFromJson(jc.value("success", json()), c.successColor);
        c.errorColor = colorFromJson(jc.value("error", json()), c.errorColor);
        c.timelineBackground = colorFromJson(jc.value("timeline_background", json()), c.timelineBackground);
        c.timelineGrid = colorFromJson(jc.value("timeline_grid", json()), c.timelineGrid);
        c.timelineTrackAlt = colorFromJson(jc.value("timeline_track_alt", json()), c.timelineTrackAlt);
        c.timelineRuler = colorFromJson(jc.value("timeline_ruler", json()), c.timelineRuler);
        c.selectionColor = colorFromJson(jc.value("selection", json()), c.selectionColor);
        c.hoverColor = colorFromJson(jc.value("hover", json()), c.hoverColor);
        c.ghostColor = colorFromJson(jc.value("ghost", json()), c.ghostColor);
        c.edgeColor = colorFromJson(jc.value("edge", json()), c.edgeColor);
        c.nodeOutline = colorFromJson(jc.value("node_outline", json()), c.nodeOutline);
        c.blockColor = colorFromJson(jc.value("block", json()), c.blockColor);
        c.groupColors.clear();
        if (jc.contains("group_overrides") && jc["group_overrides"].is_object()) {
            for (auto it = jc["group_overrides"].begin(); it != jc["group_overrides"].end(); ++it)
                c.groupColors[it.key()] = colorFromJson(it.value(), ImVec4(1, 1, 1, 1));
        }
    }
    return true;
}

}  // namespace se::theme
