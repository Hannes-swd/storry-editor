// Timeline window (spec 3.1): tracks per group/element, events as points on a
// non linear time axis, drag & drop, filters.
#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_stdlib.h"

#include "core/StoryTime.h"
#include "ui/Dialogs.h"
#include "ui/Editor.h"
#include "ui/Theme.h"
#include "ui/UiCommon.h"
#include "ui/Windows.h"

namespace se {
namespace {

// --------------------------------------------------------------- time axis
struct TimeAxis {
    std::vector<long long> keys;
    std::vector<float> xs;
    float pxPerMinute = 0.01f;
    float width = 100.0f;

    void build(std::vector<long long> times, float pxPerMin, float minGap, float maxGap,
               bool compress) {
        pxPerMinute = pxPerMin;
        keys.clear();
        xs.clear();
        std::sort(times.begin(), times.end());
        times.erase(std::unique(times.begin(), times.end()), times.end());
        if (times.empty()) {
            keys.push_back(0);
            xs.push_back(0.0f);
            width = 600.0f;
            return;
        }
        keys = times;
        xs.resize(keys.size());
        xs[0] = 0.0f;
        for (size_t i = 1; i < keys.size(); ++i) {
            float raw = static_cast<float>(keys[i] - keys[i - 1]) * pxPerMinute;
            float px = raw;
            if (compress) px = std::min(std::max(raw, minGap), maxGap);
            px = std::max(px, 2.0f);
            xs[i] = xs[i - 1] + px;
        }
        width = xs.back() + 220.0f;
    }

    bool isCompressed(size_t segment, float* rawOut, float* actualOut) const {
        if (segment == 0 || segment >= keys.size()) return false;
        float raw = static_cast<float>(keys[segment] - keys[segment - 1]) * pxPerMinute;
        float actual = xs[segment] - xs[segment - 1];
        if (rawOut) *rawOut = raw;
        if (actualOut) *actualOut = actual;
        return raw > actual + 1.0f;
    }

    float timeToX(long long t) const {
        if (keys.empty()) return 0.0f;
        if (t <= keys.front()) return xs.front() + static_cast<float>(t - keys.front()) * pxPerMinute;
        if (t >= keys.back()) return xs.back() + static_cast<float>(t - keys.back()) * pxPerMinute;
        size_t hi = static_cast<size_t>(
            std::lower_bound(keys.begin(), keys.end(), t) - keys.begin());
        if (hi == 0) return xs.front();
        size_t lo = hi - 1;
        long long span = keys[hi] - keys[lo];
        float f = span > 0 ? static_cast<float>(t - keys[lo]) / static_cast<float>(span) : 0.0f;
        return xs[lo] + (xs[hi] - xs[lo]) * f;
    }

    long long xToTime(float x) const {
        if (keys.empty()) return 0;
        if (x <= xs.front())
            return keys.front() + static_cast<long long>((x - xs.front()) / pxPerMinute);
        if (x >= xs.back())
            return keys.back() + static_cast<long long>((x - xs.back()) / pxPerMinute);
        size_t hi = static_cast<size_t>(std::lower_bound(xs.begin(), xs.end(), x) - xs.begin());
        if (hi == 0) return keys.front();
        size_t lo = hi - 1;
        float span = xs[hi] - xs[lo];
        float f = span > 0.0f ? (x - xs[lo]) / span : 0.0f;
        return keys[lo] + static_cast<long long>(f * static_cast<float>(keys[hi] - keys[lo]));
    }
};

// ------------------------------------------------------------------- rows
struct TrackRow {
    std::string id;
    bool isGroup = false;
    int depth = 0;
    std::string label;
    ImVec4 color;
    std::vector<const Action*> events;
};

struct TimelineState {
    TimeUnit unit = TimeUnit::Day;
    float zoom = 1.0f;
    float headerWidth = 210.0f;
    std::set<std::string> groupFilter;    // root group ids, empty = all
    std::set<std::string> typeFilter;     // action types, empty = all
    std::string elementSearch;
    bool useTimeFilter = false;
    std::string fromText = "Tag 1, 00:00";
    std::string toText = "Tag 30, 00:00";
    std::string ctxActionId;
    std::string dragActionId;
    long long dragTime = 0;
    int dragRow = -1;
    bool dragActive = false;
    bool initialised = false;
    float pendingScrollX = -1.0f;
};

TimelineState& state() {
    static TimelineState s;
    return s;
}

bool actionMatchesFilters(Editor& ed, const Action& a, TimelineState& st, long long fromT,
                          long long toT) {
    if (!st.typeFilter.empty() && st.typeFilter.count(a.type) == 0) return false;
    if (st.useTimeFilter) {
        long long t = ed.project.resolveActionTime(a);
        if (t < fromT || t > toT) return false;
    }
    return true;
}

bool elementVisible(Editor& ed, const Element& el, TimelineState& st) {
    if (!st.elementSearch.empty() && !iequalsContains(el.name, st.elementSearch)) return false;
    if (!st.groupFilter.empty()) {
        std::string root = ed.project.rootGroupOf(el.groupId);
        if (st.groupFilter.count(root) == 0) return false;
    }
    return true;
}

void collectSubtreeEvents(Editor& ed, const std::string& groupId,
                          const std::vector<const Action*>& pool, std::vector<const Action*>& out) {
    for (const Action* a : pool) {
        for (const std::string& elId : a->elementIds) {
            const Element* el = ed.project.element(elId);
            if (el && ed.project.isAncestorGroup(groupId, el->groupId)) {
                out.push_back(a);
                break;
            }
        }
    }
}

void buildRows(Editor& ed, TimelineState& st, const std::string& parentId, int depth,
               const std::vector<const Action*>& pool, std::vector<TrackRow>& rows) {
    for (Group* g : ed.project.childGroups(parentId)) {
        if (!st.groupFilter.empty() && ed.project.rootGroupOf(g->id) != "" &&
            st.groupFilter.count(ed.project.rootGroupOf(g->id)) == 0)
            continue;

        TrackRow row;
        row.id = g->id;
        row.isGroup = true;
        row.depth = depth;
        row.label = g->name;
        row.color = ed.project.groupColor(g->id);
        if (!g->expanded) collectSubtreeEvents(ed, g->id, pool, row.events);
        rows.push_back(row);

        if (!g->expanded) continue;
        buildRows(ed, st, g->id, depth + 1, pool, rows);
        for (Element* el : ed.project.groupElements(g->id)) {
            if (!elementVisible(ed, *el, st)) continue;
            TrackRow er;
            er.id = el->id;
            er.isGroup = false;
            er.depth = depth + 1;
            er.label = el->name;
            er.color = ed.project.elementColor(el->id);
            for (const Action* a : pool) {
                bool hit = std::find(a->elementIds.begin(), a->elementIds.end(), el->id) !=
                           a->elementIds.end();
                if (!hit) {
                    for (const Mutation& m : a->mutations) {
                        if (m.elementId == el->id) hit = true;
                    }
                }
                if (hit) er.events.push_back(a);
            }
            rows.push_back(er);
        }
    }
}

void drawFilterBar(Editor& ed, TimelineState& st) {
    int unitCount = 0;
    const char* const* units = timeUnitLabels(&unitCount);
    int unitIndex = static_cast<int>(st.unit);
    ImGui::SetNextItemWidth(110.0f);
    if (ImGui::Combo("Einheit", &unitIndex, units, unitCount)) st.unit = static_cast<TimeUnit>(unitIndex);
    ui::tooltip("Basis-Zeiteinheit der Skala");

    ImGui::SameLine();
    ImGui::SetNextItemWidth(160.0f);
    ImGui::SliderFloat("Zoom", &st.zoom, 0.05f, 20.0f, "%.2fx", ImGuiSliderFlags_Logarithmic);

    ImGui::SameLine();
    ImGui::Checkbox("Komprimieren", &theme::settings().timelineCompressGaps);
    ui::tooltip(
        "Lange Zeitraeume ohne Ereignisse werden zusammengeschoben, dichte Bereiche gedehnt.");

    ImGui::SameLine();
    bool groupFilterActive = !st.groupFilter.empty();
    if (groupFilterActive) ImGui::PushStyleColor(ImGuiCol_Button, theme::accentFill());
    if (ImGui::Button("Gruppen-Filter")) ImGui::OpenPopup("tl_groups");
    if (groupFilterActive) ImGui::PopStyleColor();
    if (ImGui::BeginPopup("tl_groups")) {
        ImGui::TextUnformatted("Sichtbare Hauptgruppen (Mehrfachauswahl)");
        ImGui::Separator();
        for (const Group& g : ed.project.groups) {
            if (!g.parentId.empty()) continue;
            bool on = st.groupFilter.empty() || st.groupFilter.count(g.id) > 0;
            ui::colorDot(ed.project.groupColor(g.id));
            if (ImGui::Checkbox(g.name.c_str(), &on)) {
                if (st.groupFilter.empty()) {
                    for (const Group& o : ed.project.groups) {
                        if (o.parentId.empty()) st.groupFilter.insert(o.id);
                    }
                }
                if (on)
                    st.groupFilter.insert(g.id);
                else
                    st.groupFilter.erase(g.id);
            }
        }
        if (ImGui::Button("Alle")) st.groupFilter.clear();
        ImGui::EndPopup();
    }

    // second row: the filters
    bool typeFilterActive = !st.typeFilter.empty();
    if (typeFilterActive) ImGui::PushStyleColor(ImGuiCol_Button, theme::accentFill());
    if (ImGui::Button("Typ-Filter")) ImGui::OpenPopup("tl_types");
    if (typeFilterActive) ImGui::PopStyleColor();
    if (ImGui::BeginPopup("tl_types")) {
        for (const std::string& t : ed.project.actionTypes) {
            bool on = st.typeFilter.count(t) > 0;
            if (ImGui::Checkbox(t.c_str(), &on)) {
                if (on)
                    st.typeFilter.insert(t);
                else
                    st.typeFilter.erase(t);
            }
        }
        if (ImGui::Button("Alle")) st.typeFilter.clear();
        ImGui::EndPopup();
    }

    ImGui::SameLine();
    ImGui::SetNextItemWidth(150.0f);
    ImGui::InputTextWithHint("##search", "Element suchen...", &st.elementSearch);

    ImGui::SameLine();
    ImGui::Checkbox("Zeitfenster", &st.useTimeFilter);
    if (st.useTimeFilter) {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        ImGui::InputText("##from", &st.fromText);
        ImGui::SameLine();
        ImGui::TextUnformatted("-");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        ImGui::InputText("##to", &st.toText);
    }

    ImGui::SameLine();
    if (ImGui::Button("Filter zuruecksetzen")) {
        st.groupFilter.clear();
        st.typeFilter.clear();
        st.elementSearch.clear();
        st.useTimeFilter = false;
    }
}

}  // namespace

void drawTimelineWindow(Editor& ed, bool* open) {
    ImGui::SetNextWindowSize(ImVec2(1100, 460), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Timeline", open)) {
        ImGui::End();
        return;
    }
    if (!ed.project.loaded) {
        ui::textSecondary("Kein Projekt geoeffnet. Datei -> Neues Projekt.");
        ImGui::End();
        return;
    }

    TimelineState& st = state();
    AppSettings& settings = theme::settings();
    ColorScheme& c = theme::colors();
    if (!st.initialised) {
        st.headerWidth = settings.timelineHeaderWidth;
        st.initialised = true;
    }

    drawFilterBar(ed, st);
    ImGui::Separator();

    long long fromT = 0, toT = 0;
    if (st.useTimeFilter) {
        if (!parseStoryTime(st.fromText, &fromT)) fromT = 0;
        if (!parseStoryTime(st.toText, &toT)) toT = fromT + 30 * kMinutesPerDay;
    }

    std::vector<const Action*> pool;
    for (const Action* a : ed.project.sortedActions()) {
        if (actionMatchesFilters(ed, *a, st, fromT, toT)) pool.push_back(a);
    }

    std::vector<TrackRow> rows;
    buildRows(ed, st, "", 0, pool, rows);

    std::vector<long long> times;
    for (const TrackRow& r : rows) {
        for (const Action* a : r.events) times.push_back(ed.project.resolveActionTime(*a));
    }
    if (times.empty()) {
        for (const Action* a : pool) times.push_back(ed.project.resolveActionTime(*a));
    }

    const float pxPerMinute =
        (72.0f * st.zoom) / static_cast<float>(minutesPerUnit(st.unit));
    TimeAxis axis;
    axis.build(times, pxPerMinute, settings.timelineMinGapPx, settings.timelineMaxGapPx,
               settings.timelineCompressGaps);

    const float trackH = settings.timelineTrackHeight;
    const float rulerH = 28.0f;
    const float headerW = st.headerWidth;
    const float totalW = headerW + axis.width;
    const float totalH = rulerH + trackH * static_cast<float>(rows.size()) + 40.0f;

    ImGui::BeginChild("tl_canvas", ImVec2(0, 0), ImGuiChildFlags_Borders,
                      ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoMove);

    ImVec2 origin = ImGui::GetCursorScreenPos();
    ImVec2 winPos = ImGui::GetWindowPos();
    ImVec2 winSize = ImGui::GetWindowSize();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImGui::Dummy(ImVec2(totalW, totalH));
    const bool canvasHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);
    const ImVec2 mouse = ImGui::GetMousePos();

    if (st.pendingScrollX >= 0.0f) {
        ImGui::SetScrollX(st.pendingScrollX);
        st.pendingScrollX = -1.0f;
    }

    auto xOf = [&](long long t) { return origin.x + headerW + axis.timeToX(t); };
    auto rowTop = [&](int i) { return origin.y + rulerH + trackH * static_cast<float>(i); };

    // ---------------------------------------------------------- background
    dl->AddRectFilled(winPos, ImVec2(winPos.x + winSize.x, winPos.y + winSize.y),
                      theme::u32(c.timelineBackground));

    ImVec2 clipMin(winPos.x + headerW, winPos.y + rulerH);
    ImVec2 clipMax(winPos.x + winSize.x, winPos.y + winSize.y);
    dl->PushClipRect(clipMin, clipMax, true);

    for (size_t i = 0; i < rows.size(); ++i) {
        float y = rowTop(static_cast<int>(i));
        if (i % 2 == 1) {
            dl->AddRectFilled(ImVec2(origin.x + headerW, y),
                              ImVec2(origin.x + totalW, y + trackH), theme::u32(c.timelineTrackAlt));
        }
        dl->AddLine(ImVec2(origin.x + headerW, y + trackH), ImVec2(origin.x + totalW, y + trackH),
                    theme::u32(c.timelineGrid, 0.4f));
    }

    // vertical grid at unit boundaries + compressed gap markers
    if (!axis.keys.empty()) {
        long long unitMin = minutesPerUnit(st.unit);
        long long first = (axis.keys.front() / unitMin) * unitMin;
        long long last = axis.keys.back() + unitMin;
        float lastLabelX = -1e9f;
        int guard = 0;
        for (long long t = first; t <= last && guard < 4000; t += unitMin, ++guard) {
            float x = xOf(t);
            if (x < winPos.x + headerW - 50.0f || x > winPos.x + winSize.x + 50.0f) continue;
            dl->AddLine(ImVec2(x, origin.y + rulerH), ImVec2(x, origin.y + totalH),
                        theme::u32(c.timelineGrid, 0.35f));
            if (x - lastLabelX > 70.0f) lastLabelX = x;
        }
        for (size_t s = 1; s < axis.keys.size(); ++s) {
            float raw = 0.0f, actual = 0.0f;
            if (!axis.isCompressed(s, &raw, &actual)) continue;
            float x0 = xOf(axis.keys[s - 1]);
            float x1 = xOf(axis.keys[s]);
            float mid = (x0 + x1) * 0.5f;
            std::string label = "~ " + formatDuration(axis.keys[s] - axis.keys[s - 1]);
            ImVec2 size = ImGui::CalcTextSize(label.c_str());
            if (x1 - x0 > size.x + 10.0f) {
                float y = origin.y + rulerH + 2.0f;
                dl->AddText(ImVec2(mid - size.x * 0.5f, y), theme::u32(c.textSecondary),
                            label.c_str());
            }
            dl->AddLine(ImVec2(mid - 4.0f, origin.y + rulerH), ImVec2(mid + 4.0f, origin.y + totalH),
                        theme::u32(c.timelineGrid, 0.8f), 1.0f);
        }
    }

    // ------------------------------------------------------------- markers
    const float markerR = std::min(6.0f, trackH * 0.28f);
    const Action* hoveredAction = nullptr;
    int hoveredRow = -1;
    long long hoveredTime = 0;

    for (size_t i = 0; i < rows.size(); ++i) {
        const TrackRow& row = rows[i];
        float y = rowTop(static_cast<int>(i)) + trackH * 0.5f;

        // connecting line between the first and the last event of the track
        if (row.events.size() > 1) {
            float x0 = xOf(ed.project.resolveActionTime(*row.events.front()));
            float x1 = xOf(ed.project.resolveActionTime(*row.events.back()));
            dl->AddLine(ImVec2(x0, y), ImVec2(x1, y), theme::u32(row.color, 0.45f), 2.0f);
        }

        float lastX = -1e9f;
        int stagger = 0;
        for (const Action* a : row.events) {
            long long t = ed.project.resolveActionTime(*a);
            float x = xOf(t);
            if (x < winPos.x + headerW - 30.0f || x > winPos.x + winSize.x + 30.0f) continue;
            if (x - lastX < markerR * 2.2f)
                stagger = (stagger + 1) % 3;
            else
                stagger = 0;
            lastX = x;
            float my = y + (stagger == 0 ? 0.0f : (stagger == 1 ? -markerR * 1.6f : markerR * 1.6f));
            if (st.dragActive && st.dragActionId == a->id) continue;

            bool isSel = ed.selection.is(SelKind::Action, a->id);
            float dist = std::sqrt((mouse.x - x) * (mouse.x - x) + (mouse.y - my) * (mouse.y - my));
            bool hot = canvasHovered && dist <= markerR + 3.0f;
            if (hot) {
                hoveredAction = a;
                hoveredRow = static_cast<int>(i);
                hoveredTime = t;
            }

            float r = markerR * (hot ? 1.45f : 1.0f);
            dl->AddCircleFilled(ImVec2(x, my), r, theme::u32(row.color));
            if (a->major)
                dl->AddCircle(ImVec2(x, my), r + 2.0f, theme::u32(c.warningColor), 0, 1.5f);
            if (isSel)
                dl->AddCircle(ImVec2(x, my), r + 3.0f, theme::u32(c.selectionColor), 0, 2.0f);
            else if (hot)
                dl->AddCircle(ImVec2(x, my), r + 2.0f, theme::u32(c.hoverColor), 0, 1.5f);
        }
    }

    // ghost while dragging
    if (st.dragActive && !st.dragActionId.empty()) {
        float gx = std::max(mouse.x, winPos.x + headerW + 2.0f);
        long long t = axis.xToTime(gx - origin.x - headerW);
        st.dragTime = t;
        int targetRow = static_cast<int>((mouse.y - origin.y - rulerH) / trackH);
        if (targetRow >= 0 && targetRow < static_cast<int>(rows.size())) st.dragRow = targetRow;
        float gy = rowTop(st.dragRow >= 0 ? st.dragRow : 0) + trackH * 0.5f;
        dl->AddCircleFilled(ImVec2(gx, gy), markerR * 1.3f, theme::u32(c.ghostColor));
        dl->AddLine(ImVec2(gx, origin.y + rulerH), ImVec2(gx, origin.y + totalH),
                    theme::u32(c.ghostColor), 1.0f);
        std::string label = formatStoryTime(t);
        dl->AddText(ImVec2(gx + 8.0f, gy - 18.0f), theme::u32(c.textPrimary), label.c_str());
    }

    dl->PopClipRect();

    // ------------------------------------------------- pinned ruler on top
    dl->PushClipRect(ImVec2(winPos.x, winPos.y), clipMax, true);
    float rulerY = winPos.y;
    dl->AddRectFilled(ImVec2(winPos.x, rulerY), ImVec2(winPos.x + winSize.x, rulerY + rulerH),
                      theme::u32(theme::darken(c.panelBackground, 0.03f)));
    dl->AddLine(ImVec2(winPos.x, rulerY + rulerH), ImVec2(winPos.x + winSize.x, rulerY + rulerH),
                theme::u32(c.timelineGrid));
    if (!axis.keys.empty()) {
        long long unitMin = minutesPerUnit(st.unit);
        long long first = (axis.keys.front() / unitMin) * unitMin;
        long long last = axis.keys.back() + unitMin;
        float lastLabelX = -1e9f;
        int guard = 0;
        for (long long t = first; t <= last && guard < 4000; t += unitMin, ++guard) {
            float x = xOf(t);
            if (x < winPos.x + headerW || x > winPos.x + winSize.x) continue;
            dl->AddLine(ImVec2(x, rulerY + rulerH - 6.0f), ImVec2(x, rulerY + rulerH),
                        theme::u32(c.timelineRuler));
            std::string label = st.unit == TimeUnit::Minute || st.unit == TimeUnit::Hour
                                    ? formatStoryTime(t)
                                    : formatDayHeadline(t);
            float w = ImGui::CalcTextSize(label.c_str()).x;
            if (x - lastLabelX > w + 18.0f) {
                dl->AddText(ImVec2(x + 3.0f, rulerY + 5.0f), theme::u32(c.timelineRuler),
                            label.c_str());
                lastLabelX = x;
            }
        }
    }
    dl->PopClipRect();

    // ------------------------------------------- pinned track header column
    dl->PushClipRect(ImVec2(winPos.x, winPos.y + rulerH),
                     ImVec2(winPos.x + headerW, winPos.y + winSize.y), true);
    dl->AddRectFilled(ImVec2(winPos.x, winPos.y + rulerH),
                      ImVec2(winPos.x + headerW, winPos.y + winSize.y),
                      theme::u32(c.panelBackground));
    dl->PopClipRect();

    ImGui::PushClipRect(ImVec2(winPos.x, winPos.y + rulerH),
                        ImVec2(winPos.x + headerW, winPos.y + winSize.y), true);
    for (size_t i = 0; i < rows.size(); ++i) {
        TrackRow& row = rows[i];
        float y = rowTop(static_cast<int>(i));
        if (y + trackH < winPos.y + rulerH || y > winPos.y + winSize.y) continue;

        float indent = 6.0f + static_cast<float>(row.depth) * 14.0f;
        ImGui::SetCursorScreenPos(ImVec2(winPos.x + indent, y + (trackH - ImGui::GetTextLineHeight()) * 0.5f));
        ImGui::PushID(row.id.c_str());

        if (row.isGroup) {
            Group* g = ed.project.group(row.id);
            bool expanded = g && g->expanded;
            if (ImGui::ArrowButton("##toggle", expanded ? ImGuiDir_Down : ImGuiDir_Right)) {
                if (g) {
                    g->expanded = !g->expanded;
                    ed.markMetadata();
                }
            }
            ImGui::SameLine(0.0f, 4.0f);
        } else {
            ImGui::Dummy(ImVec2(6.0f, 1.0f));
            ImGui::SameLine(0.0f, 4.0f);
        }
        ui::colorDot(row.color);
        bool selected = ed.selection.is(row.isGroup ? SelKind::Group : SelKind::Element, row.id);
        if (ImGui::Selectable((row.label + "##rowsel").c_str(), selected,
                              ImGuiSelectableFlags_AllowDoubleClick,
                              ImVec2(headerW - indent - 40.0f, 0))) {
            ed.select(row.isGroup ? SelKind::Group : SelKind::Element, row.id);
            if (!row.isGroup) ed.focusElementId = row.id;
        }
        if (ImGui::BeginPopupContextItem("##rowctx")) {
            if (!row.isGroup && ImGui::MenuItem("Neue Aktion fuer dieses Element"))
                dialogs::openNewAction(ed, times.empty() ? 0 : times.front(), row.id);
            if (row.isGroup && ImGui::MenuItem("Neues Element")) dialogs::openNewElement(ed, row.id);
            if (ImGui::MenuItem("Details anzeigen")) {
                ed.select(row.isGroup ? SelKind::Group : SelKind::Element, row.id);
                theme::settings().showDetails = true;
            }
            ImGui::EndPopup();
        }
        if (!row.events.empty()) {
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, c.textSecondary);
            ImGui::Text("%d", static_cast<int>(row.events.size()));
            ImGui::PopStyleColor();
        }
        ImGui::PopID();
    }
    ImGui::PopClipRect();

    // resizable splitter between header column and timeline area
    ImGui::SetCursorScreenPos(ImVec2(winPos.x + headerW - 3.0f, winPos.y + rulerH));
    ImGui::InvisibleButton("##splitter", ImVec2(6.0f, std::max(20.0f, winSize.y - rulerH)));
    if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    if (ImGui::IsItemActive()) {
        st.headerWidth = std::min(500.0f, std::max(120.0f, st.headerWidth + ImGui::GetIO().MouseDelta.x));
        settings.timelineHeaderWidth = st.headerWidth;
    }
    dl->AddLine(ImVec2(winPos.x + headerW, winPos.y + rulerH),
                ImVec2(winPos.x + headerW, winPos.y + winSize.y), theme::u32(c.timelineGrid), 1.5f);

    // --------------------------------------------------------- interaction
    bool overCanvasArea = canvasHovered && mouse.x > winPos.x + headerW && mouse.y > winPos.y + rulerH;

    if (hoveredAction) {
        ImGui::BeginTooltip();
        ImGui::PushStyleColor(ImGuiCol_Text, ed.project.colorForId(
                                                 hoveredAction->elementIds.empty()
                                                     ? std::string()
                                                     : hoveredAction->elementIds.front()));
        ImGui::TextUnformatted(hoveredAction->title.c_str());
        ImGui::PopStyleColor();
        ui::textSecondary(formatStoryTimeLong(hoveredTime).c_str());
        if (!hoveredAction->description.empty()) {
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 24.0f);
            ImGui::TextUnformatted(ui::ellipsis(hoveredAction->description, 100).c_str());
            ImGui::PopTextWrapPos();
        }
        if (!hoveredAction->elementIds.empty()) {
            std::string names;
            for (size_t i = 0; i < hoveredAction->elementIds.size(); ++i) {
                if (i) names += ", ";
                names += ed.project.displayName(hoveredAction->elementIds[i]);
            }
            ui::textSecondary(("Beteiligt: " + names).c_str());
        }
        if (!hoveredAction->tags.empty())
            ui::textSecondary(("Tags: " + joinList(hoveredAction->tags)).c_str());
        ImGui::EndTooltip();
    }

    if (overCanvasArea && hoveredAction && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        ed.select(SelKind::Action, hoveredAction->id);
        ed.focusActionId = hoveredAction->id;
        theme::settings().showDetails = true;
        st.dragActionId = hoveredAction->id;
        st.dragRow = hoveredRow;
    }
    if (overCanvasArea && hoveredAction && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        dialogs::openEditAction(ed, hoveredAction->id);
        st.dragActionId.clear();
        st.dragActive = false;
    }
    if (!st.dragActionId.empty() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 4.0f))
        st.dragActive = true;

    if (st.dragActive && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        Action* a = ed.project.action(st.dragActionId);
        if (a) {
            ed.pushUndo("Aktion verschoben");
            a->time = st.dragTime;
            a->useRelative = false;
            a->relativeToId.clear();
            // dropping on another element track re-assigns the participant
            if (st.dragRow >= 0 && st.dragRow < static_cast<int>(rows.size())) {
                const TrackRow& target = rows[st.dragRow];
                if (!target.isGroup &&
                    std::find(a->elementIds.begin(), a->elementIds.end(), target.id) ==
                        a->elementIds.end()) {
                    a->elementIds.push_back(target.id);
                }
            }
            ed.markActions();
            ed.setStatus("Aktion verschoben auf " + formatStoryTime(a->time));
        }
        st.dragActive = false;
        st.dragActionId.clear();
        st.dragRow = -1;
    }
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left) && !st.dragActive) st.dragActionId.clear();

    if (overCanvasArea && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        st.ctxActionId = hoveredAction ? hoveredAction->id : std::string();
        st.dragTime = axis.xToTime(mouse.x - origin.x - headerW);
        int r = static_cast<int>((mouse.y - origin.y - rulerH) / trackH);
        st.dragRow = (r >= 0 && r < static_cast<int>(rows.size())) ? r : -1;
        ImGui::OpenPopup("tl_ctx");
    }
    if (ImGui::BeginPopup("tl_ctx")) {
        if (!st.ctxActionId.empty()) {
            const Action* a = ed.project.action(st.ctxActionId);
            if (a) {
                ImGui::TextUnformatted(a->title.c_str());
                ImGui::Separator();
                if (ImGui::MenuItem("Bearbeiten")) dialogs::openEditAction(ed, st.ctxActionId);
                if (ImGui::MenuItem("Duplizieren")) {
                    ed.pushUndo("Aktion dupliziert");
                    Action copy = *a;
                    copy.id = newId("act");
                    copy.title += " (Kopie)";
                    ed.project.actions.push_back(copy);
                    ed.markActions();
                }
                if (ImGui::MenuItem("Im Aktionen-Panel zeigen")) {
                    ed.focusActionId = st.ctxActionId;
                    theme::settings().showActions = true;
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Loeschen")) {
                    std::string id = st.ctxActionId;
                    std::string title = a->title;
                    dialogs::confirm(ed, "Aktion loeschen", "Aktion \"" + title + "\" loeschen?", "",
                                     [&ed, id]() {
                                         ed.pushUndo("Aktion geloescht");
                                         ed.project.removeAction(id);
                                         ed.markActions();
                                     });
                }
            }
        } else {
            std::string elementId;
            if (st.dragRow >= 0 && st.dragRow < static_cast<int>(rows.size()) && !rows[st.dragRow].isGroup)
                elementId = rows[st.dragRow].id;
            if (ImGui::MenuItem("Neue Aktion hier")) dialogs::openNewAction(ed, st.dragTime, elementId);
            ImGui::TextDisabled("%s", formatStoryTimeLong(st.dragTime).c_str());
        }
        ImGui::EndPopup();
    }

    // double click on empty track space creates an action there
    if (overCanvasArea && !hoveredAction && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        int r = static_cast<int>((mouse.y - origin.y - rulerH) / trackH);
        std::string elementId;
        if (r >= 0 && r < static_cast<int>(rows.size()) && !rows[r].isGroup) elementId = rows[r].id;
        dialogs::openNewAction(ed, axis.xToTime(mouse.x - origin.x - headerW), elementId);
    }

    // ctrl + wheel zooms, shift + wheel scrolls horizontally
    if (canvasHovered) {
        float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0.0f && ImGui::GetIO().KeyCtrl) {
            st.zoom = std::min(20.0f, std::max(0.05f, st.zoom * (wheel > 0 ? 1.15f : 0.87f)));
        } else if (wheel != 0.0f && ImGui::GetIO().KeyShift) {
            ImGui::SetScrollX(ImGui::GetScrollX() - wheel * 60.0f);
        }
    }

    // scroll to a selected action requested by another window
    if (!ed.focusActionId.empty() && ed.focusTimeline) {
        const Action* a = ed.project.action(ed.focusActionId);
        if (a) {
            float x = axis.timeToX(ed.project.resolveActionTime(*a));
            st.pendingScrollX = std::max(0.0f, x - winSize.x * 0.4f);
        }
        ed.focusTimeline = false;
    }

    if (rows.empty()) {
        ImGui::SetCursorScreenPos(ImVec2(winPos.x + headerW + 20.0f, winPos.y + rulerH + 20.0f));
        ui::textSecondary("Keine Spuren sichtbar - Gruppen anlegen oder Filter zuruecksetzen.");
    }

    ImGui::EndChild();
    ImGui::End();
}

}  // namespace se
