#include "ui/Lang.h"
#include "ui/Theme.h"

#include <algorithm>

#include <nlohmann/json.hpp>

#include "app/Platform.h"
#include "core/Model.h"

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

ImVec4 rgb(int r, int g, int b, float a = 1.0f) {
    return ImVec4(static_cast<float>(r) / 255.0f, static_cast<float>(g) / 255.0f,
                  static_cast<float>(b) / 255.0f, a);
}

}  // namespace

AppSettings& settings() {
    if (!g_initialised) {
        resetToDefault();
        g_initialised = true;
    }
    return g_settings;
}

ColorScheme& colors() { return settings().colors; }

void resetToDefault() { applyPreset(ThemePreset::Light); }

const char* presetName(ThemePreset preset) {
    return preset == ThemePreset::Light ? TR("Hell") : TR("Dunkel");
}

void applyPreset(ThemePreset preset) {
    g_initialised = true;
    g_settings.preset = preset;
    ColorScheme& c = g_settings.colors;

    if (preset == ThemePreset::Light) {
        // Papierweiss mit Graphit-Akzent - kein Blaustich.
        c.textPrimary = rgb(0x22, 0x22, 0x26);
        c.textSecondary = rgb(0x6E, 0x6E, 0x76);
        c.backgroundColor = rgb(0xF2, 0xF2, 0xEF);
        c.panelBackground = rgb(0xFF, 0xFF, 0xFF);
        c.accentColor = rgb(0x4E, 0x4E, 0x57);
        c.warningColor = rgb(0xC1, 0x7A, 0x0A);
        c.successColor = rgb(0x3E, 0x8E, 0x2F);
        c.errorColor = rgb(0xC0, 0x2A, 0x24);
        c.timelineBackground = rgb(0xFA, 0xFA, 0xF7);
        c.timelineGrid = rgb(0xD5, 0xD5, 0xCE);
        c.timelineTrackAlt = rgb(0xEC, 0xEC, 0xE7);
        c.timelineRuler = rgb(0x55, 0x55, 0x5C);
        c.selectionColor = rgb(0x1E, 0x1E, 0x22);
        c.hoverColor = rgb(0xA8, 0x72, 0x00);
        c.ghostColor = ImVec4(0.12f, 0.12f, 0.14f, 0.35f);
        c.edgeColor = rgb(0x70, 0x70, 0x78);
        c.nodeOutline = rgb(0x3A, 0x3A, 0x42);
        c.blockColor = rgb(0x6B, 0x66, 0x8C);
        c.pageColor = rgb(0xFF, 0xFF, 0xFF);
        c.pageTextColor = rgb(0x1A, 0x1A, 0x1C);
        c.workspaceColor = rgb(0xE3, 0xE3, 0xDF);
        setGroupPalette(0.70f, 0.66f);  // kraeftigere Gruppenfarben auf Weiss
    } else {
        // Neutrales Dunkelgrau mit weisser Schrift - ebenfalls ohne Blau.
        c.textPrimary = rgb(0xF2, 0xF2, 0xF4);
        c.textSecondary = rgb(0x9C, 0x9C, 0xA4);
        c.backgroundColor = rgb(0x14, 0x14, 0x16);
        c.panelBackground = rgb(0x1E, 0x1E, 0x21);
        c.accentColor = rgb(0xC4, 0xC4, 0xCC);
        c.warningColor = rgb(0xE0, 0xA3, 0x3E);
        c.successColor = rgb(0x7F, 0xC4, 0x6A);
        c.errorColor = rgb(0xE0, 0x56, 0x4E);
        c.timelineBackground = rgb(0x17, 0x17, 0x1A);
        c.timelineGrid = rgb(0x3A, 0x3A, 0x40);
        c.timelineTrackAlt = rgb(0x21, 0x21, 0x25);
        c.timelineRuler = rgb(0xC8, 0xC8, 0xD0);
        c.selectionColor = rgb(0xFF, 0xFF, 0xFF);
        c.hoverColor = rgb(0xFF, 0xE2, 0xA8);
        c.ghostColor = ImVec4(1.0f, 1.0f, 1.0f, 0.35f);
        c.edgeColor = rgb(0x9A, 0x9A, 0xA4);
        c.nodeOutline = rgb(0x0C, 0x0C, 0x0E);
        c.blockColor = rgb(0x9B, 0x96, 0xBE);
        // Das Blatt bleibt hell wie Papier - Schriftfarben im Dokument sollen in
        // beiden Designs gleich lesbar sein.
        c.pageColor = rgb(0xEC, 0xEB, 0xE7);
        c.pageTextColor = rgb(0x1A, 0x1A, 0x1C);
        c.workspaceColor = rgb(0x0E, 0x0E, 0x10);
        setGroupPalette(0.55f, 0.80f);
    }
    c.groupColors.clear();

    // Dokumentfarben wie in Word: kraeftige Grundtoene fuer die Schrift,
    // helle Leuchtfarben fuer die Hervorhebung.
    c.textPalette = {rgb(0xC0, 0x00, 0x00), rgb(0xFF, 0x00, 0x00), rgb(0xFF, 0xC0, 0x00),
                     rgb(0x92, 0xD0, 0x50), rgb(0x00, 0xB0, 0x50), rgb(0x00, 0xB0, 0xF0),
                     rgb(0x00, 0x70, 0xC0), rgb(0x00, 0x20, 0x60), rgb(0x70, 0x30, 0xA0),
                     rgb(0x7F, 0x7F, 0x7F)};
    c.highlightPalette = {rgb(0xFF, 0xFF, 0x00), rgb(0x00, 0xFF, 0x00), rgb(0x00, 0xFF, 0xFF),
                          rgb(0xFF, 0x00, 0xFF), rgb(0xFF, 0x00, 0x00), rgb(0xC0, 0xC0, 0xC0)};  // Words Textmarkerfarben
}

Fonts& fonts() {
    static Fonts f;
    return f;
}

ImFont* fontFor(bool bold, bool italic) {
    Fonts& f = fonts();
    ImFont* wanted = nullptr;
    if (bold && italic) wanted = f.boldItalic;
    else if (bold) wanted = f.bold;
    else if (italic) wanted = f.italic;
    if (!wanted && bold) wanted = f.bold;
    if (!wanted && italic) wanted = f.italic;
    return wanted ? wanted : f.regular;
}

// ------------------------------------------------------------- Schriftarten
namespace {

struct FamilyFiles {
    const char* name;
    const char* files[4];  // normal, fett, kursiv, fett-kursiv
};

const FamilyFiles kFamilies[] = {
    {"Georgia", {"georgia.ttf", "georgiab.ttf", "georgiai.ttf", "georgiaz.ttf"}},
    {"Times New Roman", {"times.ttf", "timesbd.ttf", "timesi.ttf", "timesbi.ttf"}},
    {"Cambria", {"cambria.ttc", "cambriab.ttf", "cambriai.ttf", "cambriaz.ttf"}},
    {"Palatino Linotype", {"pala.ttf", "palab.ttf", "palai.ttf", "palabi.ttf"}},
    {"Book Antiqua", {"BKANT.TTF", "ANTQUAB.TTF", "ANTQUAI.TTF", "ANTQUABI.TTF"}},
    {"Garamond", {"GARA.TTF", "GARABD.TTF", "GARAIT.TTF", ""}},
    {"Constantia", {"constan.ttf", "constanb.ttf", "constani.ttf", "constanz.ttf"}},
    {"Calibri", {"calibri.ttf", "calibrib.ttf", "calibrii.ttf", "calibriz.ttf"}},
    {"Segoe UI", {"segoeui.ttf", "segoeuib.ttf", "segoeuii.ttf", "segoeuiz.ttf"}},
    {"Arial", {"arial.ttf", "arialbd.ttf", "ariali.ttf", "arialbi.ttf"}},
    {"Verdana", {"verdana.ttf", "verdanab.ttf", "verdanai.ttf", "verdanaz.ttf"}},
    {"Candara", {"Candara.ttf", "Candarab.ttf", "Candarai.ttf", "Candaraz.ttf"}},
    {"Consolas", {"consola.ttf", "consolab.ttf", "consolai.ttf", "consolaz.ttf"}},
    {"Courier New", {"cour.ttf", "courbd.ttf", "couri.ttf", "courbi.ttf"}},
};

struct LoadedFamily {
    ImFont* fonts[4] = {nullptr, nullptr, nullptr, nullptr};
    bool requested = false;
    bool loaded = false;
};

std::map<std::string, LoadedFamily>& loadedFamilies() {
    static std::map<std::string, LoadedFamily> m;
    return m;
}

std::string fontPath(const char* file) { return std::string("C:/Windows/Fonts/") + file; }

bool fileExists(const std::string& path) { return platform::lastWriteTime(path) != 0; }

}  // namespace

ImFont* addFontWithFallback(const char* path, float size) {
    ImGuiIO& io = ImGui::GetIO();
    ImFont* font = io.Fonts->AddFontFromFileTTF(path, size);
    if (!font) return nullptr;
    // Die zuerst geladene Schrift gewinnt; das Symbol-Font fuellt nur Luecken.
    const std::string symbols = fontPath("seguisym.ttf");
    if (fileExists(symbols)) {
        ImFontConfig cfg;
        cfg.MergeMode = true;
        cfg.DstFont = font;
        io.Fonts->AddFontFromFileTTF(symbols.c_str(), size, &cfg);
    }
    return font;
}

namespace {

const FamilyFiles* findFamily(const std::string& name) {
    for (const FamilyFiles& f : kFamilies) {
        if (name == f.name) return &f;
    }
    return nullptr;
}

}  // namespace

const std::vector<std::string>& fontFamilies() {
    static std::vector<std::string> names;
    static bool scanned = false;
    if (!scanned) {
        scanned = true;
        for (const FamilyFiles& f : kFamilies) {
            if (fileExists(fontPath(f.files[0]))) names.push_back(f.name);
        }
    }
    return names;
}

ImFont* familyFont(const std::string& family, bool bold, bool italic) {
    if (family.empty()) return fontFor(bold, italic);
    LoadedFamily& lf = loadedFamilies()[family];
    if (!lf.loaded) {
        lf.requested = true;
        return fontFor(bold, italic);
    }
    const int index = bold && italic ? 3 : bold ? 1 : italic ? 2 : 0;
    if (lf.fonts[index]) return lf.fonts[index];
    // Fehlt ein Schnitt, nimm den naechstbesten der Familie.
    if (bold && italic && lf.fonts[1]) return lf.fonts[1];
    if (lf.fonts[0]) return lf.fonts[0];
    return fontFor(bold, italic);
}

uint64_t& fontGenerationCounter() {
    static uint64_t g = 1;
    return g;
}

uint64_t fontGeneration() { return fontGenerationCounter(); }

void loadPendingFonts() {
    for (auto& kv : loadedFamilies()) {
        LoadedFamily& lf = kv.second;
        if (!lf.requested || lf.loaded) continue;
        lf.loaded = true;
        ++fontGenerationCounter();
        const FamilyFiles* files = findFamily(kv.first);
        if (!files) continue;
        for (int k = 0; k < 4; ++k) {
            if (!files->files[k] || !files->files[k][0]) continue;
            const std::string path = fontPath(files->files[k]);
            if (!fileExists(path)) continue;
            lf.fonts[k] = addFontWithFallback(path.c_str(), settings().fontSize);
        }
    }
}

void pageSizeCm(int format, float* width, float* height) {
    switch (format) {
        case 1: *width = 14.8f; *height = 21.0f; break;    // A5
        case 2: *width = 21.59f; *height = 27.94f; break;  // Letter
        default: *width = 21.0f; *height = 29.7f; break;   // A4
    }
}

const char* pageFormatName(int format) {
    switch (format) {
        case 1: return "A5";
        case 2: return "Letter";
        default: return "A4";
    }
}

ImU32 u32(const ImVec4& c, float alphaScale) {
    ImVec4 col = c;
    col.w *= alphaScale;
    return ImGui::ColorConvertFloat4ToU32(col);
}

ImVec4 mix(const ImVec4& a, const ImVec4& b, float t) {
    return ImVec4(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t,
                  a.w + (b.w - a.w) * t);
}

bool isLightTheme() { return isBright(colors().backgroundColor); }

ImVec4 accentFill() {
    const ColorScheme& c = colors();
    return mix(c.panelBackground, c.accentColor, isLightTheme() ? 0.38f : 0.55f);
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
    s.InputTextCursorSize = 2.0f;  // ein Strich Breite ist beim Schreiben zu duenn
    s.WindowTitleAlign = ImVec2(0.0f, 0.5f);
    s.WindowMenuButtonPosition = ImGuiDir_None;

    // Interaktive Flaechen werden aus Panel + Akzent gemischt. Dadurch bleiben
    // sie im hellen Theme hell (dunkle Schrift lesbar) und im dunklen dunkel
    // (weisse Schrift lesbar) - ohne pro Theme eigene Regeln.
    const bool light = isBright(c.backgroundColor);
    const ImVec4 base = c.panelBackground;
    auto tint = [&](float t) { return mix(base, c.accentColor, t); };
    const ImVec4 surface = light ? mix(base, c.backgroundColor, 0.55f) : mix(base, c.accentColor, 0.06f);

    ImVec4* col = s.Colors;
    col[ImGuiCol_Text] = c.textPrimary;
    col[ImGuiCol_TextDisabled] = c.textSecondary;
    col[ImGuiCol_WindowBg] = c.backgroundColor;
    col[ImGuiCol_ChildBg] = withAlpha(c.panelBackground, 0.0f);
    col[ImGuiCol_PopupBg] = light ? base : mix(base, c.backgroundColor, 0.4f);
    col[ImGuiCol_Border] = withAlpha(c.textSecondary, light ? 0.40f : 0.32f);
    col[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);
    col[ImGuiCol_FrameBg] = surface;
    col[ImGuiCol_FrameBgHovered] = tint(light ? 0.10f : 0.14f);
    col[ImGuiCol_FrameBgActive] = tint(light ? 0.16f : 0.20f);
    col[ImGuiCol_TitleBg] = mix(base, c.backgroundColor, 0.5f);
    col[ImGuiCol_TitleBgActive] = tint(light ? 0.22f : 0.30f);
    col[ImGuiCol_TitleBgCollapsed] = mix(base, c.backgroundColor, 0.7f);
    col[ImGuiCol_MenuBarBg] = mix(base, c.backgroundColor, 0.35f);
    col[ImGuiCol_ScrollbarBg] = withAlpha(c.backgroundColor, 0.5f);
    col[ImGuiCol_ScrollbarGrab] = tint(0.22f);
    col[ImGuiCol_ScrollbarGrabHovered] = tint(0.34f);
    col[ImGuiCol_ScrollbarGrabActive] = tint(0.50f);
    col[ImGuiCol_CheckMark] = c.accentColor;
    col[ImGuiCol_SliderGrab] = tint(0.55f);
    col[ImGuiCol_SliderGrabActive] = tint(0.72f);
    col[ImGuiCol_Button] = tint(light ? 0.12f : 0.16f);
    col[ImGuiCol_ButtonHovered] = tint(light ? 0.26f : 0.32f);
    col[ImGuiCol_ButtonActive] = tint(light ? 0.38f : 0.46f);
    col[ImGuiCol_Header] = tint(light ? 0.18f : 0.24f);
    col[ImGuiCol_HeaderHovered] = tint(light ? 0.28f : 0.34f);
    col[ImGuiCol_HeaderActive] = tint(light ? 0.38f : 0.46f);
    col[ImGuiCol_Separator] = withAlpha(c.textSecondary, 0.35f);
    col[ImGuiCol_SeparatorHovered] = withAlpha(c.textSecondary, 0.65f);
    col[ImGuiCol_SeparatorActive] = c.accentColor;
    col[ImGuiCol_ResizeGrip] = withAlpha(c.textSecondary, 0.30f);
    col[ImGuiCol_ResizeGripHovered] = withAlpha(c.textSecondary, 0.60f);
    col[ImGuiCol_ResizeGripActive] = c.accentColor;
    col[ImGuiCol_Tab] = mix(base, c.backgroundColor, 0.55f);
    col[ImGuiCol_TabHovered] = tint(light ? 0.24f : 0.30f);
    col[ImGuiCol_TabSelected] = tint(light ? 0.16f : 0.22f);
    col[ImGuiCol_TabSelectedOverline] = c.accentColor;
    col[ImGuiCol_TabDimmed] = mix(base, c.backgroundColor, 0.75f);
    col[ImGuiCol_TabDimmedSelected] = mix(base, c.backgroundColor, 0.35f);
    col[ImGuiCol_TabDimmedSelectedOverline] = withAlpha(c.accentColor, 0.5f);
    col[ImGuiCol_DockingPreview] = withAlpha(c.accentColor, 0.45f);
    col[ImGuiCol_DockingEmptyBg] = mix(c.backgroundColor, base, 0.3f);
    col[ImGuiCol_TableHeaderBg] = mix(base, c.backgroundColor, 0.45f);
    col[ImGuiCol_TableBorderStrong] = withAlpha(c.textSecondary, 0.45f);
    col[ImGuiCol_TableBorderLight] = withAlpha(c.textSecondary, 0.22f);
    col[ImGuiCol_TableRowBg] = withAlpha(c.panelBackground, 0.0f);
    col[ImGuiCol_TableRowBgAlt] = light ? withAlpha(c.textSecondary, 0.07f)
                                        : withAlpha(c.accentColor, 0.05f);
    col[ImGuiCol_TextSelectedBg] = withAlpha(c.accentColor, light ? 0.28f : 0.40f);
    // Ohne diese Zeile bleibt der Schreibcursor auf ImGuis Vorgabe (weiss) und
    // ist im hellen Theme unsichtbar.
    col[ImGuiCol_InputTextCursor] = c.textPrimary;
    col[ImGuiCol_NavCursor] = c.accentColor;
    col[ImGuiCol_ModalWindowDimBg] = light ? ImVec4(0.15f, 0.15f, 0.16f, 0.35f)
                                           : ImVec4(0.0f, 0.0f, 0.0f, 0.55f);
}

bool save() {
    AppSettings& s = settings();
    json j;
    j["language"] = lang::code(s.language);
    j["preset"] = presetName(s.preset);
    j["font_size"] = s.fontSize;
    j["timeline"] = {{"track_height", s.timelineTrackHeight},
                     {"header_width", s.timelineHeaderWidth},
                     {"min_gap", s.timelineMinGapPx},
                     {"max_gap", s.timelineMaxGapPx},
                     {"compress_gaps", s.timelineCompressGaps}};
    j["autosave"] = s.autosave;
    j["document"] = {{"font", s.docFont},
                     {"font_size", s.docFontSize},
                     {"line_spacing", s.docLineSpacing},
                     {"margin_cm", s.docMarginCm},
                     {"margin_left_cm", s.docMarginLeftCm},
                     {"margin_right_cm", s.docMarginRightCm},
                     {"page_format", s.docPageFormat},
                     {"page_view", s.docPageView},
                     {"zoom", s.docZoom},
                     {"ruler", s.docShowRuler},
                     {"outline", s.docShowOutline},
                     {"marks", s.docShowMarks},
                     {"ribbon_collapsed", s.docRibbonCollapsed},
                     {"ribbon_tab", s.docRibbonTab},
                     {"spell_check", s.docSpellCheck},
                     {"auto_correct", s.docAutoCorrect},
                     {"language", s.docLanguage}};
    j["last_vault"] = s.lastVault;
    j["last_project_name"] = s.lastProjectName;
    j["windows"] = {{"manuscript", s.showManuscript}, {"timeline", s.showTimeline}, {"groups", s.showGroups},
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
    jc["page"] = colorToJson(c.pageColor);
    jc["page_text"] = colorToJson(c.pageTextColor);
    jc["workspace"] = colorToJson(c.workspaceColor);
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

    // Dateien aus einer Version vor den Designs ("preset" fehlt) bringen noch die
    // alte dunkelblaue Palette mit - die wird verworfen, damit das neue helle
    // Standarddesign greift.
    s.language = lang::fromCode(j.value("language", std::string(lang::code(s.language))));
    lang::set(s.language);
    const bool hasPreset = j.contains("preset");
    const std::string preset = j.value("preset", std::string(TR("Hell")));
    applyPreset(preset == TR("Dunkel") ? ThemePreset::Dark : ThemePreset::Light);
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
    if (j.contains("document")) {
        const json& d = j["document"];
        s.docFont = d.value("font", s.docFont);
        s.docFontSize = std::clamp(d.value("font_size", s.docFontSize), 6.0f, 72.0f);
        s.docLineSpacing = std::clamp(d.value("line_spacing", s.docLineSpacing), 0.8f, 3.0f);
        s.docMarginCm = std::clamp(d.value("margin_cm", s.docMarginCm), 0.5f, 6.0f);
        // Aeltere Einstellungen kennen nur einen Rand fuer alle Seiten.
        s.docMarginLeftCm = std::clamp(d.value("margin_left_cm", s.docMarginCm), 0.0f, 10.0f);
        s.docMarginRightCm = std::clamp(d.value("margin_right_cm", s.docMarginCm), 0.0f, 10.0f);
        s.docPageFormat = std::clamp(d.value("page_format", s.docPageFormat), 0, 2);
        s.docPageView = d.value("page_view", s.docPageView);
        s.docZoom = std::clamp(d.value("zoom", s.docZoom), 0.3f, 4.0f);
        s.docShowRuler = d.value("ruler", s.docShowRuler);
        s.docShowOutline = d.value("outline", s.docShowOutline);
        s.docShowMarks = d.value("marks", s.docShowMarks);
        s.docRibbonCollapsed = d.value("ribbon_collapsed", s.docRibbonCollapsed);
        s.docRibbonTab = std::clamp(d.value("ribbon_tab", s.docRibbonTab), 0, 5);
        s.docSpellCheck = d.value("spell_check", s.docSpellCheck);
        s.docAutoCorrect = d.value("auto_correct", s.docAutoCorrect);
        s.docLanguage = d.value("language", s.docLanguage);
    }
    s.lastVault = j.value("last_vault", s.lastVault);
    s.lastProjectName = j.value("last_project_name", s.lastProjectName);
    if (j.contains("windows")) {
        const json& w = j["windows"];
        s.showManuscript = w.value("manuscript", s.showManuscript);
        s.showTimeline = w.value("timeline", s.showTimeline);
        s.showGroups = w.value("groups", s.showGroups);
        s.showActions = w.value("actions", s.showActions);
        s.showStory = w.value("story", s.showStory);
        s.showConnections = w.value("connections", s.showConnections);
        s.showFiles = w.value("files", s.showFiles);
        s.showDetails = w.value("details", s.showDetails);
        s.showSettings = w.value("settings", s.showSettings);
    }
    if (hasPreset && j.contains("colors")) {
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
        c.pageColor = colorFromJson(jc.value("page", json()), c.pageColor);
        c.pageTextColor = colorFromJson(jc.value("page_text", json()), c.pageTextColor);
        c.workspaceColor = colorFromJson(jc.value("workspace", json()), c.workspaceColor);
        c.groupColors.clear();
        if (jc.contains("group_overrides") && jc["group_overrides"].is_object()) {
            for (auto it = jc["group_overrides"].begin(); it != jc["group_overrides"].end(); ++it)
                c.groupColors[it.key()] = colorFromJson(it.value(), ImVec4(1, 1, 1, 1));
        }
    }
    return true;
}

}  // namespace se::theme
