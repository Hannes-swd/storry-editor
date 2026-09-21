// Connections window (spec 3.4): network graph of permanent relationships.
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
#include "ui/Lang.h"
#include "ui/Theme.h"
#include "ui/UiCommon.h"
#include "ui/Windows.h"

namespace se {
namespace {

struct ConnState {
    ImVec2 pan = ImVec2(0, 0);
    float zoom = 1.0f;
    std::vector<std::string> selected;   // element ids
    std::string selectedConnection;
    std::set<std::string> typeFilter;   // Typ-IDs
    std::string focusId;
    bool useTimeSlider = false;
    long long sliderTime = 0;
    int focusDegree = 1;
    std::string dragNode;
    bool panning = false;
};

ConnState& state() {
    static ConnState s;
    return s;
}

void ensurePositions(Editor& ed, const std::vector<const Element*>& nodes, const ImVec2& canvas) {
    int missing = 0;
    for (const Element* el : nodes) {
        if (ed.project.nodePositions.find(el->id) == ed.project.nodePositions.end()) ++missing;
    }
    if (missing == 0) return;
    // ring that still fits into the visible canvas
    float wanted = 70.0f + 24.0f * static_cast<float>(nodes.size());
    float fits = std::max(80.0f, std::min(canvas.x, canvas.y) * 0.35f);
    float radius = std::min(wanted, fits);
    int index = 0;
    for (const Element* el : nodes) {
        if (ed.project.nodePositions.find(el->id) != ed.project.nodePositions.end()) {
            ++index;
            continue;
        }
        float angle = 6.2831853f * static_cast<float>(index) / std::max<size_t>(1, nodes.size());
        ed.project.nodePositions[el->id] =
            ImVec2(std::cos(angle) * radius, std::sin(angle) * radius);
        ++index;
    }
    ed.markMetadata();
}

bool withinFocus(Editor& ed, const std::string& elementId, const ConnState& st) {
    if (st.focusId.empty()) return true;
    if (elementId == st.focusId) return true;
    std::set<std::string> frontier{st.focusId};
    std::set<std::string> visited{st.focusId};
    for (int d = 0; d < st.focusDegree; ++d) {
        std::set<std::string> next;
        for (const Connection& conn : ed.project.connections) {
            bool touches = false;
            for (const std::string& m : conn.members) {
                if (frontier.count(m)) touches = true;
            }
            if (!touches) continue;
            for (const std::string& m : conn.members) next.insert(m);
        }
        for (const std::string& id : next) visited.insert(id);
        frontier = next;
    }
    return visited.count(elementId) > 0;
}

float distanceToSegment(const ImVec2& p, const ImVec2& a, const ImVec2& b) {
    ImVec2 ab(b.x - a.x, b.y - a.y);
    ImVec2 ap(p.x - a.x, p.y - a.y);
    float len2 = ab.x * ab.x + ab.y * ab.y;
    float t = len2 > 0.0f ? std::max(0.0f, std::min(1.0f, (ap.x * ab.x + ap.y * ab.y) / len2)) : 0.0f;
    ImVec2 proj(a.x + ab.x * t, a.y + ab.y * t);
    return std::sqrt((p.x - proj.x) * (p.x - proj.x) + (p.y - proj.y) * (p.y - proj.y));
}

}  // namespace

void drawConnectionsWindow(Editor& ed, bool* open) {
    ImGui::SetNextWindowSize(ImVec2(760, 560), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(TWIN("Connections", "connections"), open)) {
        ImGui::End();
        return;
    }
    if (!ed.project.loaded) {
        ui::textSecondary(TR("Kein Projekt geoeffnet."));
        ImGui::End();
        return;
    }

    ConnState& st = state();
    ColorScheme& c = theme::colors();

    // ------------------------------------------------------------- toolbar
    bool canConnect = st.selected.size() == 2;
    if (!canConnect) ImGui::BeginDisabled();
    if (ImGui::Button(TR("Verbindung erstellen")))
        dialogs::openNewConnection(ed, st.selected[0], st.selected[1]);
    if (!canConnect) ImGui::EndDisabled();

    ImGui::SameLine();
    bool canBlock = st.selected.size() >= 2;
    if (!canBlock) ImGui::BeginDisabled();
    if (ImGui::Button(TR("Block erstellen"))) {
        std::vector<std::string> connIds;
        for (const Connection& conn : ed.project.connections) {
            bool all = !conn.members.empty();
            for (const std::string& m : conn.members) {
                if (std::find(st.selected.begin(), st.selected.end(), m) == st.selected.end()) all = false;
            }
            if (all) connIds.push_back(conn.id);
        }
        dialogs::openNewBlock(ed, connIds);
    }
    if (!canBlock) ImGui::EndDisabled();
    ui::tooltip(TR("Fasst alle Verbindungen zwischen den markierten Knoten zu einem Block zusammen."));

    ImGui::SameLine();
    if (ImGui::Button(TR("Auswahl leeren"))) {
        st.selected.clear();
        st.selectedConnection.clear();
    }
    ImGui::SameLine();
    if (ImGui::Button(TR("Typen verwalten..."))) dialogs::openConnectionTypes(ed);
    ImGui::SameLine();
    if (ImGui::Button(TR("Auto-Layout"))) {
        ed.pushUndo(TR("Auto-Layout"));
        ed.project.nodePositions.clear();
        ed.markMetadata();
    }
    ImGui::SameLine();
    bool filterActive = !st.typeFilter.empty();
    if (filterActive) ImGui::PushStyleColor(ImGuiCol_Button, theme::accentFill());
    if (ImGui::Button(TR("Typ-Filter"))) ImGui::OpenPopup("conn_types");
    if (filterActive) ImGui::PopStyleColor();
    if (ImGui::BeginPopup("conn_types")) {
        for (const ConnectionType& t : ed.project.connectionTypes) {
            bool on = st.typeFilter.count(t.id) > 0;
            if (ImGui::Checkbox((t.name + "##" + t.id).c_str(), &on)) {
                if (on)
                    st.typeFilter.insert(t.id);
                else
                    st.typeFilter.erase(t.id);
            }
        }
        if (ImGui::Button(TR("Alle"))) st.typeFilter.clear();
        ImGui::EndPopup();
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(180.0f);
    ui::elementCombo(ed, TR("Fokus"), st.focusId, true);
    if (!st.focusId.empty()) {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(90.0f);
        ImGui::SliderInt(TR("Grad"), &st.focusDegree, 1, 3);
    }

    if (!ed.project.blocks.empty()) {
        ImGui::SameLine();
        if (ImGui::Button(TR("Bloecke"))) ImGui::OpenPopup("conn_blocks");
        if (ImGui::BeginPopup("conn_blocks")) {
            for (Block& b : ed.project.blocks) {
                ImGui::PushID(b.id.c_str());
                if (ImGui::Checkbox("##collapsed", &b.collapsed)) ed.markConnections();
                ImGui::SameLine();
                ImGui::TextUnformatted(b.name.c_str());
                ImGui::SameLine();
                if (ImGui::SmallButton(TR("Loeschen"))) {
                    ed.pushUndo(TR("Block geloescht"));
                    ed.project.removeBlock(b.id);
                    ed.markConnections();
                    ImGui::PopID();
                    break;
                }
                ImGui::PopID();
            }
            ImGui::EndPopup();
        }
    }

    bool anyTemporal = false;
    for (const ConnectionType& t : ed.project.connectionTypes) {
        if (t.temporal) anyTemporal = true;
    }

    // -------------------------------------------------------------- canvas
    const float barHeight = anyTemporal ? (st.useTimeSlider ? 92.0f : 34.0f) : 0.0f;
    ImGui::BeginChild("graph", ImVec2(0, -barHeight), ImGuiChildFlags_Borders,
                      ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoMove);
    ImVec2 canvasPos = ImGui::GetWindowPos();
    ImVec2 canvasSize = ImGui::GetWindowSize();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y),
                      theme::u32(c.timelineBackground));

    ImVec2 center(canvasPos.x + canvasSize.x * 0.5f, canvasPos.y + canvasSize.y * 0.5f);
    auto toScreen = [&](const ImVec2& p) {
        return ImVec2(center.x + (p.x + st.pan.x) * st.zoom, center.y + (p.y + st.pan.y) * st.zoom);
    };

    std::vector<const Element*> nodes;
    for (const Element& el : ed.project.elements) {
        if (!withinFocus(ed, el.id, st)) continue;
        nodes.push_back(&el);
    }
    ensurePositions(ed, nodes, canvasSize);

    const bool hovered = ImGui::IsWindowHovered();
    const ImVec2 mouse = ImGui::GetMousePos();

    // ---------------------------------------------------------- blocks bg
    for (const Block& b : ed.project.blocks) {
        ImVec2 mn(1e9f, 1e9f), mx(-1e9f, -1e9f);
        int count = 0;
        for (const Connection& conn : ed.project.connections) {
            if (conn.blockId != b.id) continue;
            for (const std::string& id : conn.members) {
                auto it = ed.project.nodePositions.find(id);
                if (it == ed.project.nodePositions.end()) continue;
                ImVec2 p = toScreen(it->second);
                mn.x = std::min(mn.x, p.x);
                mn.y = std::min(mn.y, p.y);
                mx.x = std::max(mx.x, p.x);
                mx.y = std::max(mx.y, p.y);
                ++count;
            }
        }
        if (count == 0) continue;
        mn = ImVec2(mn.x - 34.0f, mn.y - 34.0f);
        mx = ImVec2(mx.x + 34.0f, mx.y + 34.0f);
        dl->AddRectFilled(mn, mx, theme::u32(theme::withAlpha(c.blockColor, 0.18f)), 10.0f);
        dl->AddRect(mn, mx, theme::u32(c.blockColor), 10.0f, 0, 1.5f);
        dl->AddText(ImVec2(mn.x + 8.0f, mn.y + 4.0f), theme::u32(c.blockColor), b.name.c_str());
    }

    // ------------------------------------------------------------- edges
    std::string hoveredConnection;
    for (const Connection& conn : ed.project.connections) {
        if (!st.typeFilter.empty() && st.typeFilter.count(conn.typeId) == 0) continue;
        if (st.useTimeSlider && !ed.project.connectionActiveAt(conn, st.sliderTime)) continue;
        bool anyInFocus = false;
        for (const std::string& m : conn.members) {
            if (withinFocus(ed, m, st)) anyInFocus = true;
        }
        if (!anyInFocus) continue;
        const Block* blk = nullptr;
        for (const Block& b : ed.project.blocks) {
            if (b.id == conn.blockId) blk = &b;
        }
        if (blk && blk->collapsed) continue;

        const std::string typeName = ed.project.connectionTypeName(conn);
        std::vector<ImVec2> points;
        for (const std::string& m : conn.members) {
            auto it = ed.project.nodePositions.find(m);
            if (it != ed.project.nodePositions.end()) points.push_back(toScreen(it->second));
        }
        if (points.size() < 2) continue;

        ImVec4 col = ed.project.connectionTypeColor(conn.typeId);

        // Mehr als zwei Rollen: Sternform mit Knotenpunkt in der Mitte
        if (points.size() > 2) {
            ImVec2 hub(0, 0);
            for (const ImVec2& pt : points) {
                hub.x += pt.x / static_cast<float>(points.size());
                hub.y += pt.y / static_cast<float>(points.size());
            }
            for (const ImVec2& pt : points) dl->AddLine(hub, pt, theme::u32(col), 2.0f);
            ImVec2 ts = ImGui::CalcTextSize(typeName.c_str());
            dl->AddRectFilled(ImVec2(hub.x - ts.x * 0.5f - 5.0f, hub.y - ts.y * 0.5f - 3.0f),
                              ImVec2(hub.x + ts.x * 0.5f + 5.0f, hub.y + ts.y * 0.5f + 3.0f),
                              theme::u32(theme::withAlpha(c.backgroundColor, 0.9f)), 4.0f);
            dl->AddRect(ImVec2(hub.x - ts.x * 0.5f - 5.0f, hub.y - ts.y * 0.5f - 3.0f),
                        ImVec2(hub.x + ts.x * 0.5f + 5.0f, hub.y + ts.y * 0.5f + 3.0f),
                        theme::u32(col), 4.0f);
            dl->AddText(ImVec2(hub.x - ts.x * 0.5f, hub.y - ts.y * 0.5f), theme::u32(col),
                        typeName.c_str());
            if (hovered && distanceToSegment(mouse, hub, points[0]) < 6.0f) hoveredConnection = conn.id;
            continue;
        }

        ImVec2 a = points[0];
        ImVec2 b = points[1];
        bool selected = st.selectedConnection == conn.id;
        float dist = distanceToSegment(mouse, a, b);
        bool hot = hovered && dist < 6.0f;
        if (hot) hoveredConnection = conn.id;
        dl->AddLine(a, b, theme::u32(selected ? c.selectionColor : col), selected ? 3.5f : (hot ? 3.0f : 2.0f));

        // arrow head
        ImVec2 dir(b.x - a.x, b.y - a.y);
        float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
        if (len > 1.0f) {
            dir.x /= len;
            dir.y /= len;
            ImVec2 tip(b.x - dir.x * 22.0f, b.y - dir.y * 22.0f);
            ImVec2 n(-dir.y, dir.x);
            dl->AddTriangleFilled(tip, ImVec2(tip.x - dir.x * 9.0f + n.x * 5.0f, tip.y - dir.y * 9.0f + n.y * 5.0f),
                                  ImVec2(tip.x - dir.x * 9.0f - n.x * 5.0f, tip.y - dir.y * 9.0f - n.y * 5.0f),
                                  theme::u32(col));
        }
        ImVec2 mid((a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f);
        ImVec2 ts = ImGui::CalcTextSize(typeName.c_str());
        dl->AddRectFilled(ImVec2(mid.x - ts.x * 0.5f - 3.0f, mid.y - ts.y * 0.5f - 1.0f),
                          ImVec2(mid.x + ts.x * 0.5f + 3.0f, mid.y + ts.y * 0.5f + 1.0f),
                          theme::u32(theme::withAlpha(c.backgroundColor, 0.85f)), 3.0f);
        dl->AddText(ImVec2(mid.x - ts.x * 0.5f, mid.y - ts.y * 0.5f), theme::u32(col),
                    typeName.c_str());
    }

    // ------------------------------------------------------------- nodes
    const float nodeR = 20.0f * st.zoom;
    std::string hoveredNode;
    for (const Element* el : nodes) {
        auto it = ed.project.nodePositions.find(el->id);
        if (it == ed.project.nodePositions.end()) continue;
        ImVec2 p = toScreen(it->second);
        if (p.x < canvasPos.x - 80.0f || p.x > canvasPos.x + canvasSize.x + 80.0f) continue;
        if (p.y < canvasPos.y - 80.0f || p.y > canvasPos.y + canvasSize.y + 80.0f) continue;

        float dist = std::sqrt((mouse.x - p.x) * (mouse.x - p.x) + (mouse.y - p.y) * (mouse.y - p.y));
        bool hot = hovered && dist <= nodeR;
        if (hot) hoveredNode = el->id;
        bool selected = std::find(st.selected.begin(), st.selected.end(), el->id) != st.selected.end();

        ImVec4 col = ed.project.elementColor(el->id);
        int degree = static_cast<int>(ed.project.connectionsForElement(el->id).size());
        float r = nodeR * (1.0f + std::min(0.6f, static_cast<float>(degree) * 0.08f));
        dl->AddCircleFilled(p, r, theme::u32(col));
        dl->AddCircle(p, r, theme::u32(selected ? c.selectionColor : c.nodeOutline), 0,
                      selected ? 2.5f : 1.5f);
        if (hot) dl->AddCircle(p, r + 3.0f, theme::u32(c.hoverColor), 0, 1.5f);

        ImVec2 ts = ImGui::CalcTextSize(el->name.c_str());
        dl->AddText(ImVec2(p.x - ts.x * 0.5f, p.y + r + 3.0f), theme::u32(c.textPrimary),
                    el->name.c_str());

        // Was gilt gerade? Zeitliche Verbindungen unter dem Namen anzeigen.
        float lineY = p.y + r + 3.0f + ts.y;
        const long long when = st.useTimeSlider ? st.sliderTime : 0;
        for (const ConnectionType& type : ed.project.connectionTypes) {
            if (!type.temporal) continue;
            const size_t bandRole = static_cast<size_t>(type.bandRole < 0 ? 0 : type.bandRole);
            const size_t labelRole = static_cast<size_t>(type.labelRole < 0 ? 0 : type.labelRole);
            for (const Connection& conn : ed.project.connections) {
                if (conn.typeId != type.id) continue;
                if (bandRole >= conn.members.size() || conn.members[bandRole] != el->id) continue;
                if (st.useTimeSlider && !ed.project.connectionActiveAt(conn, when)) continue;
                if (!st.useTimeSlider && conn.hasEnd) continue;  // ohne Schieber nur das Aktuelle
                std::string target = labelRole < conn.members.size()
                                         ? ed.project.displayName(conn.members[labelRole])
                                         : std::string();
                std::string line = type.name + ": " + target;
                ImVec2 lts = ImGui::CalcTextSize(line.c_str());
                dl->AddText(ImVec2(p.x - lts.x * 0.5f, lineY),
                            theme::u32(ed.project.connectionTypeColor(type.id)), line.c_str());
                lineY += lts.y;
                break;  // pro Typ eine Zeile
            }
        }
    }

    if (!hoveredNode.empty()) {
        const Element* el = ed.project.element(hoveredNode);
        if (el) {
            ImGui::BeginTooltip();
            ImGui::TextUnformatted(el->name.c_str());
            ui::textSecondary(ed.project.elementPath(el->id).c_str());
            for (const Connection* conn : ed.project.connectionsForElement(el->id)) {
                std::string others;
                for (const std::string& m : conn->members) {
                    if (m == el->id) continue;
                    if (!others.empty()) others += ", ";
                    others += ed.project.displayName(m);
                }
                std::string when;
                if (conn->hasStart) when = " (" + formatStoryTime(conn->startTime) + ")";
                ImGui::BulletText("%s %s%s", ed.project.connectionTypeName(*conn).c_str(),
                                  others.c_str(), when.c_str());
            }
            ImGui::EndTooltip();
        }
    }

    // -------------------------------------------------------- interaction
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        if (!hoveredNode.empty()) {
            if (!ImGui::GetIO().KeyCtrl) st.selected.clear();
            auto it = std::find(st.selected.begin(), st.selected.end(), hoveredNode);
            if (it == st.selected.end())
                st.selected.push_back(hoveredNode);
            else
                st.selected.erase(it);
            ed.select(SelKind::Element, hoveredNode);
            st.dragNode = hoveredNode;
        } else if (!hoveredConnection.empty()) {
            st.selectedConnection = hoveredConnection;
            ed.select(SelKind::Connection, hoveredConnection);
        } else {
            st.panning = true;
            if (!ImGui::GetIO().KeyCtrl) st.selected.clear();
        }
    }
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        if (!st.dragNode.empty()) ed.markMetadata();
        st.dragNode.clear();
        st.panning = false;
    }
    if (!st.dragNode.empty() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        ImVec2 delta = ImGui::GetIO().MouseDelta;
        ImVec2& pos = ed.project.nodePositions[st.dragNode];
        pos.x += delta.x / st.zoom;
        pos.y += delta.y / st.zoom;
    } else if (st.panning && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        ImVec2 delta = ImGui::GetIO().MouseDelta;
        st.pan.x += delta.x / st.zoom;
        st.pan.y += delta.y / st.zoom;
    }
    if (hovered && ImGui::GetIO().MouseWheel != 0.0f) {
        st.zoom = std::min(3.0f, std::max(0.25f, st.zoom * (ImGui::GetIO().MouseWheel > 0 ? 1.1f : 0.9f)));
    }

    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        st.selectedConnection = hoveredConnection;
        if (!hoveredNode.empty() &&
            std::find(st.selected.begin(), st.selected.end(), hoveredNode) == st.selected.end())
            st.selected.push_back(hoveredNode);
        ImGui::OpenPopup("conn_ctx");
    }
    if (ImGui::BeginPopup("conn_ctx")) {
        if (!st.selectedConnection.empty()) {
            const Connection* conn = ed.project.connection(st.selectedConnection);
            if (conn) {
                ImGui::TextDisabled("%s", ed.project.connectionTypeName(*conn).c_str());
                if (ImGui::MenuItem(TR("Verbindung bearbeiten")))
                    dialogs::openEditConnection(ed, st.selectedConnection);
                if (ImGui::MenuItem(TR("Verbindung loeschen"))) {
                    std::string id = st.selectedConnection;
                    dialogs::confirm(ed, TR("Verbindung loeschen"), TR("Diese Verbindung loeschen?"), "",
                                     [&ed, id]() {
                                         ed.pushUndo(TR("Verbindung geloescht"));
                                         ed.project.removeConnection(id);
                                         ed.markConnections();
                                     });
                }
                ImGui::Separator();
            }
        }
        if (st.selected.size() == 2) {
            if (ImGui::MenuItem(TR("Die zwei markierten verbinden")))
                dialogs::openNewConnection(ed, st.selected[0], st.selected[1]);
        }
        if (st.selected.size() == 1) {
            if (ImGui::MenuItem(TR("Verbindung von hier...")))
                dialogs::openNewConnection(ed, st.selected[0], "");
            if (ImGui::MenuItem(TR("Details anzeigen"))) {
                ed.select(SelKind::Element, st.selected[0]);
                theme::settings().showDetails = true;
            }
        }
        if (ImGui::MenuItem(TR("Auswahl leeren"))) st.selected.clear();
        ImGui::EndPopup();
    }

    if (nodes.empty())
        dl->AddText(ImVec2(canvasPos.x + 20.0f, canvasPos.y + 20.0f), theme::u32(c.textSecondary),
                    TR("Keine Elemente vorhanden."));
    else if (ed.project.connectionTypes.empty())
        dl->AddText(ImVec2(canvasPos.x + 20.0f, canvasPos.y + 20.0f), theme::u32(c.textSecondary),
                    TR("Zuerst einen Verbindungstyp anlegen."));
    else
        dl->AddText(ImVec2(canvasPos.x + 8.0f, canvasPos.y + canvasSize.y - 20.0f),
                    theme::u32(c.textSecondary),
                    TR("Linksklick = waehlen (Strg = mehrere), ziehen = verschieben, Rad = Zoom"));

    ImGui::EndChild();

    // ------------------------------------------------------- Zeitleiste
    // Zugeklappt gelten alle Verbindungen, aufgeklappt nur die, die zum
    // gewaehlten Zeitpunkt tatsaechlich gelten.
    if (anyTemporal) {
        // Die Beschriftung darf nicht vom Zustand der Vorframe abhaengen, sonst
        // haengt sie beim Auf- und Zuklappen einen Frame hinterher.
        const bool barOpen = ImGui::CollapsingHeader(TR("Zeitleiste"));
        st.useTimeSlider = barOpen;
        if (barOpen) {
            ui::textSecondary(TR("Es werden nur Verbindungen gezeigt, die an diesem Tag gelten."));
            long long maxTime = 7 * kMinutesPerDay;
            for (const Action& a : ed.project.actions)
                maxTime = std::max(maxTime, ed.project.resolveActionTime(a));
            for (const Connection& conn : ed.project.connections) {
                if (conn.hasStart) maxTime = std::max(maxTime, conn.startTime);
                if (conn.hasEnd) maxTime = std::max(maxTime, conn.endTime);
            }
            const int lastDay = static_cast<int>(dayOf(maxTime)) + 1;
            int day = static_cast<int>(dayOf(st.sliderTime));

            if (ImGui::Button("|<")) day = 1;
            ui::tooltip(TR("Zum Anfang"));
            ImGui::SameLine();
            if (ImGui::Button("<") && day > 1) --day;
            ImGui::SameLine();
            if (ImGui::Button(">") && day < lastDay) ++day;
            ImGui::SameLine();
            if (ImGui::Button(">|")) day = lastDay;
            ui::tooltip(TR("Zum Ende"));
            ImGui::SameLine();
            ImGui::SetNextItemWidth(-160.0f);
            ImGui::SliderInt("##timeline", &day, 1, lastDay,
                             formatDayHeadline(static_cast<long long>(day - 1) * kMinutesPerDay)
                                 .c_str());
            st.sliderTime = static_cast<long long>(day - 1) * kMinutesPerDay;

            ImGui::SameLine();
            int active = 0;
            for (const Connection& conn : ed.project.connections) {
                if (ed.project.connectionActiveAt(conn, st.sliderTime)) ++active;
            }
            ImGui::PushStyleColor(ImGuiCol_Text, c.textSecondary);
            ImGui::Text("%d / %d", active, static_cast<int>(ed.project.connections.size()));
            ImGui::PopStyleColor();
            ui::tooltip(TR("Gueltige Verbindungen an diesem Tag"));
        }
    }

    ImGui::End();
}

}  // namespace se
