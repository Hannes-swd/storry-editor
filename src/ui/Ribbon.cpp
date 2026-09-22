#include "ui/Ribbon.h"

#include <algorithm>
#include <cfloat>
#include <cmath>

#include "imgui_internal.h"

#include "ui/Theme.h"

namespace se::ribbon {
namespace {

float g_bodyTop = 0.0f;
float g_bodyLeft = 0.0f;

// Punkt im Symbolfeld, in Anteilen von Breite und Hoehe.
ImVec2 P(ImVec2 a, ImVec2 b, float fx, float fy) {
    return ImVec2(a.x + (b.x - a.x) * fx, a.y + (b.y - a.y) * fy);
}

void letter(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col, const char* text, ImFont* font,
            float scale = 0.95f, float dx = 0.0f, float dy = 0.0f) {
    if (!font) font = ImGui::GetFont();
    const float px = (b.y - a.y) * scale;
    const ImVec2 ts = font->CalcTextSizeA(px, FLT_MAX, 0.0f, text);
    const ImVec2 at((a.x + b.x - ts.x) * 0.5f + dx * (b.x - a.x), (a.y + b.y - ts.y) * 0.5f + dy * (b.y - a.y));
    dl->AddText(font, px, at, col, text);
}

ImFont* regular() { return theme::fonts().regular ? theme::fonts().regular : ImGui::GetFont(); }

void lines(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col, const float (*spans)[2], int count,
           float x0 = 0.0f) {
    const float step = (b.y - a.y) / static_cast<float>(count + 1);
    for (int i = 0; i < count; ++i) {
        const float y = a.y + step * static_cast<float>(i + 1);
        dl->AddLine(ImVec2(a.x + (b.x - a.x) * (x0 + spans[i][0] * (1.0f - x0)), y),
                    ImVec2(a.x + (b.x - a.x) * (x0 + spans[i][1] * (1.0f - x0)), y), col, 1.5f);
    }
}

// Hintergrund eines Werkzeugknopfs je nach Zustand.
void toolFrame(ImDrawList* dl, ImVec2 a, ImVec2 b, bool hovered, bool held, bool active) {
    const ColorScheme& c = theme::colors();
    if (active)
        dl->AddRectFilled(a, b, theme::u32(theme::accentFill()), 3.0f);
    if (held)
        dl->AddRectFilled(a, b, ImGui::GetColorU32(ImGuiCol_ButtonActive), 3.0f);
    else if (hovered)
        dl->AddRectFilled(a, b, ImGui::GetColorU32(ImGuiCol_ButtonHovered), 3.0f);
    if (active) dl->AddRect(a, b, theme::u32(theme::withAlpha(c.accentColor, 0.55f)), 3.0f);
}

void tip(const char* text) {
    if (text && *text && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        ImGui::SetTooltip("%s", text);
}

}  // namespace

float rowHeight() { return ImGui::GetFrameHeight(); }

float bodyHeight() {
    return rowHeight() * 2.0f + ImGui::GetStyle().ItemSpacing.y + ImGui::GetTextLineHeight() + 6.0f;
}

bool tabStrip(int* current, const char* const* labels, int count) {
    const ColorScheme& c = theme::colors();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    bool changed = false;
    const float padX = ImGui::GetStyle().FramePadding.x * 1.6f;
    for (int i = 0; i < count; ++i) {
        if (i) ImGui::SameLine(0.0f, 2.0f);
        const ImVec2 ts = ImGui::CalcTextSize(labels[i]);
        const ImVec2 size(ts.x + padX * 2.0f, ImGui::GetFrameHeight());
        const ImVec2 p = ImGui::GetCursorScreenPos();
        ImGui::PushID(i);
        const bool pressed = ImGui::InvisibleButton("##tab", size);
        ImGui::PopID();
        const bool hovered = ImGui::IsItemHovered();
        if (pressed && *current != i) {
            *current = i;
            changed = true;
        }
        const bool selected = *current == i;
        if (hovered && !selected)
            dl->AddRectFilled(p, ImVec2(p.x + size.x, p.y + size.y), ImGui::GetColorU32(ImGuiCol_ButtonHovered), 3.0f);
        dl->AddText(ImVec2(p.x + padX, p.y + (size.y - ts.y) * 0.5f),
                    theme::u32(selected ? c.textPrimary : c.textSecondary), labels[i]);
        if (selected) {
            const float w = ts.x * 0.7f;
            const float cx = p.x + size.x * 0.5f;
            dl->AddLine(ImVec2(cx - w * 0.5f, p.y + size.y - 2.0f), ImVec2(cx + w * 0.5f, p.y + size.y - 2.0f),
                        theme::u32(c.accentColor), 3.0f);
        }
    }
    return changed;
}

void beginBody() {
    g_bodyTop = ImGui::GetCursorScreenPos().y;
    g_bodyLeft = ImGui::GetCursorScreenPos().x;
}

void endBody() {
    ImGui::NewLine();
    ImGui::SetCursorScreenPos(ImVec2(g_bodyLeft, g_bodyTop + bodyHeight()));
    ImGui::Dummy(ImVec2(0.0f, 0.0f));
}

void beginGroup() { ImGui::BeginGroup(); }

void endGroup(const char* label) {
    ImGui::EndGroup();
    const ImVec2 mn = ImGui::GetItemRectMin();
    const ImVec2 mx = ImGui::GetItemRectMax();
    const ColorScheme& c = theme::colors();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float labelY = g_bodyTop + rowHeight() * 2.0f + ImGui::GetStyle().ItemSpacing.y + 3.0f;
    const ImVec2 ts = ImGui::CalcTextSize(label);
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    const float right = std::max(mx.x, mn.x + ts.x);
    dl->AddText(ImVec2((mn.x + right - ts.x) * 0.5f, labelY), theme::u32(c.textSecondary), label);
    const float sepX = right + spacing + 2.0f;
    dl->AddLine(ImVec2(sepX, g_bodyTop + 2.0f), ImVec2(sepX, g_bodyTop + bodyHeight() - 4.0f),
                theme::u32(theme::withAlpha(c.textSecondary, 0.3f)));
    if (right > mx.x) {
        ImGui::SameLine(0.0f, 0.0f);
        ImGui::Dummy(ImVec2(right - mx.x, 1.0f));
    }
    ImGui::SameLine(0.0f, spacing * 2.0f + 5.0f);
}

bool tool(const char* id, IconFn icon, const char* tipText, bool active, bool enabled) {
    const float h = rowHeight();
    const ImVec2 p = ImGui::GetCursorScreenPos();
    if (!enabled) ImGui::BeginDisabled();
    const bool pressed = ImGui::InvisibleButton(id, ImVec2(h, h));
    if (!enabled) ImGui::EndDisabled();
    const ImVec2 q(p.x + h, p.y + h);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    toolFrame(dl, p, q, ImGui::IsItemHovered(), ImGui::IsItemActive(), active);
    const float pad = h * 0.2f;
    const ImU32 col = theme::u32(enabled ? theme::colors().textPrimary
                                         : theme::withAlpha(theme::colors().textSecondary, 0.6f));
    icon(dl, ImVec2(p.x + pad, p.y + pad), ImVec2(q.x - pad, q.y - pad), col);
    tip(tipText);
    return pressed;
}

bool colorTool(const char* id, IconFn icon, ImU32 bar, const char* tipText) {
    const float h = rowHeight();
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const bool pressed = ImGui::InvisibleButton(id, ImVec2(h, h));
    const ImVec2 q(p.x + h, p.y + h);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    toolFrame(dl, p, q, ImGui::IsItemHovered(), ImGui::IsItemActive(), false);
    const float pad = h * 0.18f;
    icon(dl, ImVec2(p.x + pad, p.y + pad * 0.7f), ImVec2(q.x - pad, q.y - pad * 2.0f),
         theme::u32(theme::colors().textPrimary));
    dl->AddRectFilled(ImVec2(p.x + pad, q.y - pad * 1.5f), ImVec2(q.x - pad, q.y - pad * 0.6f), bar);
    tip(tipText);
    return pressed;
}

bool dropArrow(const char* id, const char* tipText) {
    const float h = rowHeight();
    const float w = h * 0.5f;
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const bool pressed = ImGui::InvisibleButton(id, ImVec2(w, h));
    const ImVec2 q(p.x + w, p.y + h);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    toolFrame(dl, p, q, ImGui::IsItemHovered(), ImGui::IsItemActive(), false);
    const float cx = (p.x + q.x) * 0.5f, cy = (p.y + q.y) * 0.5f, s = w * 0.22f;
    dl->AddTriangleFilled(ImVec2(cx - s, cy - s * 0.4f), ImVec2(cx + s, cy - s * 0.4f),
                          ImVec2(cx, cy + s * 0.6f), theme::u32(theme::colors().textSecondary));
    tip(tipText);
    return pressed;
}

bool big(const char* id, IconFn icon, const char* label, const char* tipText, bool active,
         bool dropdown) {
    const float h = rowHeight() * 2.0f + ImGui::GetStyle().ItemSpacing.y;
    const ImVec2 ts = ImGui::CalcTextSize(label);
    const float w = std::max(h * 0.95f, ts.x + ImGui::GetStyle().FramePadding.x * 2.0f + (dropdown ? 10.0f : 0.0f));
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const bool pressed = ImGui::InvisibleButton(id, ImVec2(w, h));
    const ImVec2 q(p.x + w, p.y + h);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    toolFrame(dl, p, q, ImGui::IsItemHovered(), ImGui::IsItemActive(), active);
    const float iconSize = h - ts.y - 10.0f;
    const ImVec2 ia((p.x + q.x - iconSize) * 0.5f, p.y + 4.0f);
    icon(dl, ImVec2(ia.x + iconSize * 0.08f, ia.y + iconSize * 0.08f),
         ImVec2(ia.x + iconSize * 0.92f, ia.y + iconSize * 0.92f), theme::u32(theme::colors().textPrimary));
    const float tx = (p.x + q.x - ts.x - (dropdown ? 10.0f : 0.0f)) * 0.5f;
    dl->AddText(ImVec2(tx, q.y - ts.y - 3.0f), theme::u32(theme::colors().textPrimary), label);
    if (dropdown) {
        const float cx = tx + ts.x + 7.0f, cy = q.y - ts.y * 0.5f - 3.0f, s = 3.5f;
        dl->AddTriangleFilled(ImVec2(cx - s, cy - s * 0.5f), ImVec2(cx + s, cy - s * 0.5f), ImVec2(cx, cy + s * 0.6f),
                              theme::u32(theme::colors().textSecondary));
    }
    tip(tipText);
    return pressed;
}

bool labeled(const char* id, IconFn icon, const char* label, const char* tipText, bool active,
             bool enabled) {
    const float h = rowHeight();
    const ImVec2 ts = ImGui::CalcTextSize(label);
    const float w = h + ts.x + ImGui::GetStyle().FramePadding.x * 1.5f;
    const ImVec2 p = ImGui::GetCursorScreenPos();
    if (!enabled) ImGui::BeginDisabled();
    const bool pressed = ImGui::InvisibleButton(id, ImVec2(w, h));
    if (!enabled) ImGui::EndDisabled();
    const ImVec2 q(p.x + w, p.y + h);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    toolFrame(dl, p, q, ImGui::IsItemHovered(), ImGui::IsItemActive(), active);
    const float pad = h * 0.2f;
    const ImU32 col = theme::u32(enabled ? theme::colors().textPrimary
                                         : theme::withAlpha(theme::colors().textSecondary, 0.6f));
    icon(dl, ImVec2(p.x + pad, p.y + pad), ImVec2(p.x + h - pad, q.y - pad), col);
    dl->AddText(ImVec2(p.x + h, p.y + (h - ts.y) * 0.5f), col, label);
    tip(tipText);
    return pressed;
}

// ------------------------------------------------------------------- Symbole
namespace icon {

void bold(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col) {
    letter(dl, a, b, col, "B", theme::fontFor(true, false), 1.05f);
}
void italic(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col) {
    letter(dl, a, b, col, "I", theme::fontFor(false, true), 1.05f);
}
void underline(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col) {
    letter(dl, a, b, col, "U", regular(), 0.95f, 0.0f, -0.08f);
    dl->AddLine(P(a, b, 0.2f, 0.98f), P(a, b, 0.8f, 0.98f), col, 1.5f);
}
void strike(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col) {
    letter(dl, a, b, col, "ab", regular(), 0.9f);
    dl->AddLine(P(a, b, 0.0f, 0.55f), P(a, b, 1.0f, 0.55f), col, 1.5f);
}
void subscript(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col) {
    letter(dl, a, b, col, "x", regular(), 0.95f, -0.12f, -0.08f);
    letter(dl, a, b, col, "2", regular(), 0.5f, 0.3f, 0.28f);
}
void superscript(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col) {
    letter(dl, a, b, col, "x", regular(), 0.95f, -0.12f, 0.06f);
    letter(dl, a, b, col, "2", regular(), 0.5f, 0.3f, -0.3f);
}
void grow(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col) {
    letter(dl, a, b, col, "A", regular(), 1.0f, -0.12f);
    dl->AddTriangleFilled(P(a, b, 0.72f, 0.3f), P(a, b, 1.0f, 0.3f), P(a, b, 0.86f, 0.08f), col);
}
void shrink(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col) {
    letter(dl, a, b, col, "A", regular(), 0.8f, -0.12f, 0.06f);
    dl->AddTriangleFilled(P(a, b, 0.72f, 0.1f), P(a, b, 1.0f, 0.1f), P(a, b, 0.86f, 0.32f), col);
}
void clearFormat(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col) {
    letter(dl, a, b, col, "A", regular(), 0.95f, -0.15f);
    const ImU32 eraser = theme::u32(theme::colors().errorColor);
    const ImVec2 pts[4] = {P(a, b, 0.55f, 0.95f), P(a, b, 0.8f, 0.6f), P(a, b, 1.05f, 0.8f), P(a, b, 0.8f, 1.12f)};
    dl->AddConvexPolyFilled(pts, 4, eraser);
}
void fontColor(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col) { letter(dl, a, b, col, "A", regular(), 1.1f); }
void highlighter(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col) {
    const ImVec2 pts[4] = {P(a, b, 0.25f, 0.75f), P(a, b, 0.7f, 0.1f), P(a, b, 0.95f, 0.3f), P(a, b, 0.5f, 0.95f)};
    dl->AddPolyline(pts, 4, col, ImDrawFlags_Closed, 1.5f);
    dl->AddTriangleFilled(P(a, b, 0.25f, 0.75f), P(a, b, 0.5f, 0.95f), P(a, b, 0.12f, 1.0f), col);
}
void bullets(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col) {
    for (int i = 0; i < 3; ++i) {
        const float y = 0.18f + 0.32f * static_cast<float>(i);
        dl->AddCircleFilled(P(a, b, 0.1f, y), (b.y - a.y) * 0.08f, col);
        dl->AddLine(P(a, b, 0.32f, y), P(a, b, 1.0f, y), col, 1.5f);
    }
}
void numbering(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col) {
    const char* nums[3] = {"1", "2", "3"};
    for (int i = 0; i < 3; ++i) {
        const float y = 0.18f + 0.32f * static_cast<float>(i);
        const float px = (b.y - a.y) * 0.36f;
        dl->AddText(regular(), px, ImVec2(a.x, P(a, b, 0, y).y - px * 0.55f), col, nums[i]);
        dl->AddLine(P(a, b, 0.32f, y), P(a, b, 1.0f, y), col, 1.5f);
    }
}
void alignLeft(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col) {
    const float s[4][2] = {{0, 1}, {0, 0.65f}, {0, 1}, {0, 0.65f}};
    lines(dl, a, b, col, s, 4);
}
void alignCenter(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col) {
    const float s[4][2] = {{0, 1}, {0.18f, 0.82f}, {0, 1}, {0.18f, 0.82f}};
    lines(dl, a, b, col, s, 4);
}
void alignRight(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col) {
    const float s[4][2] = {{0, 1}, {0.35f, 1}, {0, 1}, {0.35f, 1}};
    lines(dl, a, b, col, s, 4);
}
void alignJustify(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col) {
    const float s[4][2] = {{0, 1}, {0, 1}, {0, 1}, {0, 0.6f}};
    lines(dl, a, b, col, s, 4);
}
void lineSpacing(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col) {
    const float s[3][2] = {{0, 1}, {0, 1}, {0, 1}};
    lines(dl, a, b, col, s, 3, 0.42f);
    dl->AddLine(P(a, b, 0.15f, 0.05f), P(a, b, 0.15f, 0.95f), col, 1.5f);
    dl->AddTriangleFilled(P(a, b, 0.0f, 0.22f), P(a, b, 0.3f, 0.22f), P(a, b, 0.15f, 0.0f), col);
    dl->AddTriangleFilled(P(a, b, 0.0f, 0.78f), P(a, b, 0.3f, 0.78f), P(a, b, 0.15f, 1.0f), col);
}
void undo(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col) {
    const ImVec2 c = P(a, b, 0.55f, 0.6f);
    const float r = (b.x - a.x) * 0.38f;
    dl->PathArcTo(c, r, IM_PI * 1.05f, IM_PI * 2.3f, 12);
    dl->PathStroke(col, 0, 1.7f);
    const ImVec2 tip = ImVec2(c.x - r, c.y - r * 0.15f);
    dl->AddTriangleFilled(ImVec2(tip.x - r * 0.45f, tip.y - r * 0.2f), ImVec2(tip.x + r * 0.45f, tip.y - r * 0.2f),
                          ImVec2(tip.x, tip.y + r * 0.5f), col);
}
void redo(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col) {
    const ImVec2 c = P(a, b, 0.45f, 0.6f);
    const float r = (b.x - a.x) * 0.38f;
    dl->PathArcTo(c, r, IM_PI * 1.95f, IM_PI * 0.7f, 12);
    dl->PathStroke(col, 0, 1.7f);
    const ImVec2 tip = ImVec2(c.x + r, c.y - r * 0.15f);
    dl->AddTriangleFilled(ImVec2(tip.x - r * 0.45f, tip.y - r * 0.2f), ImVec2(tip.x + r * 0.45f, tip.y - r * 0.2f),
                          ImVec2(tip.x, tip.y + r * 0.5f), col);
}
void save(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col) {
    dl->AddRect(P(a, b, 0.05f, 0.05f), P(a, b, 0.95f, 0.95f), col, 2.0f, 0, 1.5f);
    dl->AddRectFilled(P(a, b, 0.25f, 0.05f), P(a, b, 0.72f, 0.35f), col);
    dl->AddRect(P(a, b, 0.22f, 0.55f), P(a, b, 0.78f, 0.95f), col, 0.0f, 0, 1.2f);
}
void paste(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col) {
    dl->AddRect(P(a, b, 0.1f, 0.12f), P(a, b, 0.78f, 1.0f), col, 2.0f, 0, 1.5f);
    dl->AddRectFilled(P(a, b, 0.28f, 0.0f), P(a, b, 0.6f, 0.2f), col, 2.0f);
    dl->AddRectFilled(P(a, b, 0.45f, 0.42f), P(a, b, 1.0f, 1.0f), theme::u32(theme::colors().panelBackground));
    dl->AddRect(P(a, b, 0.45f, 0.42f), P(a, b, 1.0f, 1.0f), col, 1.0f, 0, 1.3f);
}
void cut(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col) {
    const float r = (b.x - a.x) * 0.17f;
    dl->AddCircle(P(a, b, 0.22f, 0.8f), r, col, 12, 1.4f);
    dl->AddCircle(P(a, b, 0.78f, 0.8f), r, col, 12, 1.4f);
    dl->AddLine(P(a, b, 0.32f, 0.66f), P(a, b, 0.75f, 0.0f), col, 1.4f);
    dl->AddLine(P(a, b, 0.68f, 0.66f), P(a, b, 0.25f, 0.0f), col, 1.4f);
}
void copy(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col) {
    dl->AddRect(P(a, b, 0.05f, 0.0f), P(a, b, 0.65f, 0.72f), col, 1.5f, 0, 1.3f);
    dl->AddRectFilled(P(a, b, 0.35f, 0.28f), P(a, b, 0.95f, 1.0f), theme::u32(theme::colors().panelBackground));
    dl->AddRect(P(a, b, 0.35f, 0.28f), P(a, b, 0.95f, 1.0f), col, 1.5f, 0, 1.3f);
}
void find(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col) {
    dl->AddCircle(P(a, b, 0.42f, 0.42f), (b.x - a.x) * 0.3f, col, 16, 1.6f);
    dl->AddLine(P(a, b, 0.64f, 0.64f), P(a, b, 0.98f, 0.98f), col, 2.2f);
}
void replace(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col) {
    letter(dl, a, b, col, "a", regular(), 0.6f, -0.28f, -0.22f);
    letter(dl, a, b, col, "b", regular(), 0.6f, 0.28f, 0.22f);
    dl->AddLine(P(a, b, 0.2f, 0.65f), P(a, b, 0.2f, 0.9f), col, 1.3f);
    dl->AddLine(P(a, b, 0.2f, 0.9f), P(a, b, 0.5f, 0.9f), col, 1.3f);
    dl->AddTriangleFilled(P(a, b, 0.48f, 0.8f), P(a, b, 0.48f, 1.0f), P(a, b, 0.62f, 0.9f), col);
}
void selectAll(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col) {
    dl->AddRect(P(a, b, 0.0f, 0.0f), P(a, b, 1.0f, 1.0f), col, 0.0f, 0, 1.0f);
    const float s[3][2] = {{0.12f, 0.88f}, {0.12f, 0.88f}, {0.12f, 0.6f}};
    lines(dl, a, b, col, s, 3);
}
void element(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col) { letter(dl, a, b, col, "@", regular(), 1.1f); }
void value(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col) {
    letter(dl, a, b, col, "{ }", regular(), 0.9f);
    dl->AddCircleFilled(P(a, b, 0.5f, 0.55f), (b.x - a.x) * 0.07f, col);
}
void action(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col) {
    const ImVec2 pts[6] = {P(a, b, 0.6f, 0.0f), P(a, b, 0.2f, 0.55f), P(a, b, 0.48f, 0.55f),
                           P(a, b, 0.38f, 1.0f), P(a, b, 0.82f, 0.4f), P(a, b, 0.54f, 0.4f)};
    dl->AddPolyline(pts, 6, col, ImDrawFlags_Closed, 1.5f);
}
void paragraphAction(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col) {
    const float s[3][2] = {{0, 0.6f}, {0, 0.6f}, {0, 0.45f}};
    lines(dl, a, b, col, s, 3);
    const ImVec2 pts[6] = {P(a, b, 0.88f, 0.25f), P(a, b, 0.68f, 0.62f), P(a, b, 0.82f, 0.62f),
                           P(a, b, 0.76f, 0.95f), P(a, b, 1.0f, 0.52f), P(a, b, 0.86f, 0.52f)};
    dl->AddPolyline(pts, 6, col, ImDrawFlags_Closed, 1.3f);
}
void clock(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col) {
    const ImVec2 c = P(a, b, 0.5f, 0.5f);
    const float r = (b.x - a.x) * 0.46f;
    dl->AddCircle(c, r, col, 20, 1.5f);
    dl->AddLine(c, ImVec2(c.x, c.y - r * 0.65f), col, 1.5f);
    dl->AddLine(c, ImVec2(c.x + r * 0.5f, c.y + r * 0.2f), col, 1.5f);
}
void sceneBreak(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col) {
    for (int i = 0; i < 3; ++i) {
        letter(dl, a, b, col, "*", regular(), 0.8f, -0.32f + 0.32f * static_cast<float>(i), 0.12f);
    }
}
void bookmark(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col) {
    const ImVec2 pts[5] = {P(a, b, 0.2f, 0.0f), P(a, b, 0.8f, 0.0f), P(a, b, 0.8f, 1.0f),
                           P(a, b, 0.5f, 0.72f), P(a, b, 0.2f, 1.0f)};
    dl->AddPolyline(pts, 5, col, ImDrawFlags_Closed, 1.5f);
}
void comment(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col) {
    dl->AddRect(P(a, b, 0.0f, 0.05f), P(a, b, 1.0f, 0.72f), col, 3.0f, 0, 1.5f);
    dl->AddTriangleFilled(P(a, b, 0.2f, 0.72f), P(a, b, 0.45f, 0.72f), P(a, b, 0.15f, 1.0f), col);
}
void prev(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col) {
    comment(dl, P(a, b, 0.25f, 0.0f), b, col);
    dl->AddTriangleFilled(P(a, b, 0.0f, 0.38f), P(a, b, 0.22f, 0.15f), P(a, b, 0.22f, 0.6f), col);
}
void next(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col) {
    comment(dl, a, P(a, b, 0.75f, 1.0f), col);
    dl->AddTriangleFilled(P(a, b, 1.0f, 0.38f), P(a, b, 0.78f, 0.15f), P(a, b, 0.78f, 0.6f), col);
}
void trash(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col) {
    dl->AddLine(P(a, b, 0.05f, 0.18f), P(a, b, 0.95f, 0.18f), col, 1.5f);
    dl->AddRect(P(a, b, 0.35f, 0.0f), P(a, b, 0.65f, 0.18f), col, 1.0f, 0, 1.2f);
    const ImVec2 pts[4] = {P(a, b, 0.15f, 0.18f), P(a, b, 0.25f, 1.0f), P(a, b, 0.75f, 1.0f), P(a, b, 0.85f, 0.18f)};
    dl->AddPolyline(pts, 4, col, 0, 1.5f);
}
void outline(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col) {
    dl->AddRect(a, b, col, 1.0f, 0, 1.2f);
    dl->AddLine(P(a, b, 0.35f, 0.0f), P(a, b, 0.35f, 1.0f), col, 1.2f);
    const float s[3][2] = {{0.1f, 0.25f}, {0.1f, 0.25f}, {0.1f, 0.25f}};
    lines(dl, a, b, col, s, 3);
}
void ruler(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col) {
    dl->AddRect(P(a, b, 0.0f, 0.25f), P(a, b, 1.0f, 0.75f), col, 1.0f, 0, 1.3f);
    for (int i = 1; i < 6; ++i) {
        const float x = static_cast<float>(i) / 6.0f;
        dl->AddLine(P(a, b, x, 0.25f), P(a, b, x, i % 2 ? 0.45f : 0.55f), col, 1.0f);
    }
}
void marks(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col) {
    letter(dl, a, b, col, "\xC2\xB6", regular(), 1.05f);  // Absatzzeichen
}
void split(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col) {
    dl->AddRect(a, b, col, 1.0f, 0, 1.3f);
    dl->AddLine(P(a, b, 0.0f, 0.5f), P(a, b, 1.0f, 0.5f), col, 2.0f);
}
void focus(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col) {
    const float l = 0.3f;
    dl->AddLine(P(a, b, 0, 0), P(a, b, l, 0), col, 1.5f);
    dl->AddLine(P(a, b, 0, 0), P(a, b, 0, l), col, 1.5f);
    dl->AddLine(P(a, b, 1, 0), P(a, b, 1 - l, 0), col, 1.5f);
    dl->AddLine(P(a, b, 1, 0), P(a, b, 1, l), col, 1.5f);
    dl->AddLine(P(a, b, 0, 1), P(a, b, l, 1), col, 1.5f);
    dl->AddLine(P(a, b, 0, 1), P(a, b, 0, 1 - l), col, 1.5f);
    dl->AddLine(P(a, b, 1, 1), P(a, b, 1 - l, 1), col, 1.5f);
    dl->AddLine(P(a, b, 1, 1), P(a, b, 1, 1 - l), col, 1.5f);
    const float s[2][2] = {{0.25f, 0.75f}, {0.25f, 0.75f}};
    lines(dl, a, b, col, s, 2);
}
void readMode(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col) {
    const ImVec2 pts[6] = {P(a, b, 0.0f, 0.1f), P(a, b, 0.5f, 0.2f), P(a, b, 1.0f, 0.1f),
                           P(a, b, 1.0f, 0.9f), P(a, b, 0.5f, 1.0f), P(a, b, 0.0f, 0.9f)};
    dl->AddPolyline(pts, 6, col, ImDrawFlags_Closed, 1.4f);
    dl->AddLine(P(a, b, 0.5f, 0.2f), P(a, b, 0.5f, 1.0f), col, 1.2f);
}
void pageView(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col) {
    dl->AddRect(P(a, b, 0.15f, 0.0f), P(a, b, 0.85f, 1.0f), col, 1.0f, 0, 1.4f);
    const float s[3][2] = {{0.25f, 0.75f}, {0.25f, 0.75f}, {0.25f, 0.6f}};
    lines(dl, a, b, col, s, 3);
}
void webView(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col) {
    const ImVec2 c = P(a, b, 0.5f, 0.5f);
    const float r = (b.x - a.x) * 0.46f;
    dl->AddCircle(c, r, col, 20, 1.4f);
    dl->AddEllipse(c, ImVec2(r * 0.45f, r), col, 0.0f, 20, 1.2f);
    dl->AddLine(ImVec2(c.x - r, c.y), ImVec2(c.x + r, c.y), col, 1.2f);
}
void zoom(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col) {
    find(dl, a, b, col);
    dl->AddLine(P(a, b, 0.28f, 0.42f), P(a, b, 0.56f, 0.42f), col, 1.4f);
    dl->AddLine(P(a, b, 0.42f, 0.28f), P(a, b, 0.42f, 0.56f), col, 1.4f);
}
void onePage(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col) {
    dl->AddRect(P(a, b, 0.2f, 0.0f), P(a, b, 0.8f, 1.0f), col, 1.0f, 0, 1.4f);
    letter(dl, a, b, col, "1", regular(), 0.6f);
}
void pageWidth(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col) {
    dl->AddRect(P(a, b, 0.2f, 0.0f), P(a, b, 0.8f, 1.0f), col, 1.0f, 0, 1.4f);
    dl->AddLine(P(a, b, 0.0f, 0.5f), P(a, b, 1.0f, 0.5f), col, 1.3f);
    dl->AddTriangleFilled(P(a, b, 0.0f, 0.5f), P(a, b, 0.15f, 0.38f), P(a, b, 0.15f, 0.62f), col);
    dl->AddTriangleFilled(P(a, b, 1.0f, 0.5f), P(a, b, 0.85f, 0.38f), P(a, b, 0.85f, 0.62f), col);
}
void margins(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col) {
    dl->AddRect(P(a, b, 0.1f, 0.0f), P(a, b, 0.9f, 1.0f), col, 1.0f, 0, 1.4f);
    dl->AddRect(P(a, b, 0.28f, 0.18f), P(a, b, 0.72f, 0.82f), theme::u32(theme::colors().textSecondary), 0.0f, 0, 1.0f);
}
void pageSize(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col) {
    dl->AddRect(P(a, b, 0.0f, 0.15f), P(a, b, 0.6f, 1.0f), col, 1.0f, 0, 1.3f);
    dl->AddRect(P(a, b, 0.35f, 0.0f), P(a, b, 1.0f, 0.75f), col, 1.0f, 0, 1.3f);
}
void word(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col) {
    dl->AddRect(P(a, b, 0.2f, 0.0f), P(a, b, 1.0f, 1.0f), col, 1.5f, 0, 1.3f);
    dl->AddRectFilled(P(a, b, 0.0f, 0.2f), P(a, b, 0.6f, 0.8f), col, 1.5f);
    letter(dl, P(a, b, 0.0f, 0.2f), P(a, b, 0.6f, 0.8f), theme::u32(theme::colors().panelBackground), "W",
           theme::fontFor(true, false), 0.95f);
}
void help(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col) {
    dl->AddCircle(P(a, b, 0.5f, 0.5f), (b.x - a.x) * 0.48f, col, 20, 1.4f);
    letter(dl, a, b, col, "?", theme::fontFor(true, false), 0.8f);
}
void settings(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col) {
    const ImVec2 c = P(a, b, 0.5f, 0.5f);
    const float r = (b.x - a.x) * 0.3f;
    dl->AddCircle(c, r, col, 16, 1.6f);
    for (int i = 0; i < 8; ++i) {
        const float ang = static_cast<float>(i) * IM_PI / 4.0f;
        dl->AddLine(ImVec2(c.x + std::cos(ang) * r, c.y + std::sin(ang) * r),
                    ImVec2(c.x + std::cos(ang) * r * 1.6f, c.y + std::sin(ang) * r * 1.6f), col, 2.0f);
    }
}
void count(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col) {
    letter(dl, a, b, col, "123", regular(), 0.55f, 0.0f, -0.18f);
    const float s[1][2] = {{0.05f, 0.95f}};
    lines(dl, P(a, b, 0, 0.5f), b, col, s, 1);
}
void chevronUp(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col) {
    dl->AddLine(P(a, b, 0.2f, 0.65f), P(a, b, 0.5f, 0.35f), col, 1.5f);
    dl->AddLine(P(a, b, 0.5f, 0.35f), P(a, b, 0.8f, 0.65f), col, 1.5f);
}
void chevronDown(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col) {
    dl->AddLine(P(a, b, 0.2f, 0.35f), P(a, b, 0.5f, 0.65f), col, 1.5f);
    dl->AddLine(P(a, b, 0.5f, 0.65f), P(a, b, 0.8f, 0.35f), col, 1.5f);
}

}  // namespace icon
}  // namespace se::ribbon
