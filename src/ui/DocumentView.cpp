#include "ui/DocumentView.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cfloat>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string_view>

#include "imgui_internal.h"

#include "core/StoryTime.h"
#include "app/SpellCheck.h"
#include "ui/AutoCorrect.h"
#include "ui/Editor.h"
#include "ui/Lang.h"
#include "ui/Theme.h"

namespace se {
namespace {

constexpr float kPxPerCm = 96.0f / 2.54f;
constexpr float kPxPerPt = 96.0f / 72.0f;
constexpr size_t kNone = static_cast<size_t>(-1);

size_t utf8Len(unsigned char c) {
    if (c < 0x80) return 1;
    if ((c >> 5) == 0x6) return 2;
    if ((c >> 4) == 0xE) return 3;
    if ((c >> 3) == 0x1E) return 4;
    return 1;
}

bool isNameStart(unsigned char c) {
    return std::isalnum(c) || c == '_' || c == '-' || c == '/' || c >= 0x80;
}

bool isAtomicKind(LineKind k) { return k == LineKind::Time || k == LineKind::Break; }

// Erste Marke, die hinter `pos` endet. Marken ueberlappen sich nicht und sind
// sortiert - also genuegt eine binaere Suche.
size_t firstTokenAfter(const std::vector<ManuscriptToken>& toks, size_t pos) {
    size_t lo = 0, hi = toks.size();
    while (lo < hi) {
        const size_t mid = (lo + hi) / 2;
        if (toks[mid].end <= pos)
            lo = mid + 1;
        else
            hi = mid;
    }
    return lo;
}

// Zeile -> sichtbare Einheiten mit Format. Formatzeichen fallen weg, Marken
// wie Aktionen bleiben ein einziges, unteilbares Stueck.
LineModel buildModel(const std::string& text, const std::vector<ManuscriptToken>& toks,
                     const std::vector<ManuscriptLine>& lines, size_t li) {
    const ManuscriptLine& L = lines[li];
    LineModel m;
    m.kind = L.kind;
    m.level = L.level;
    m.align = L.align;
    m.format = L.format;
    if (m.atomic()) {
        m.atomicRaw = text.substr(L.begin, L.end - L.begin);
        return m;
    }
    m.prefix = text.substr(L.begin, L.contentBegin - L.begin);

    auto addChars = [&](size_t b, size_t e, const TextStyle& st, int token, int group) {
        size_t i = b;
        while (i < e) {
            const size_t n = std::min(utf8Len(static_cast<unsigned char>(text[i])), e - i);
            if (text[i] == '\r' || text[i] == '\n') {
                ++i;
                continue;
            }
            m.units.push_back({text.substr(i, n), st});
            UnitInfo info;
            info.pos = i;
            info.token = token;
            info.group = group;
            m.info.push_back(info);
            i += n;
        }
    };

    if (L.kind == LineKind::Heading) {
        addChars(L.contentBegin, L.contentEnd, TextStyle(), -1, -1);
        return m;
    }
    for (size_t k = firstTokenAfter(toks, L.contentBegin); k < toks.size(); ++k) {
        const ManuscriptToken& t = toks[k];
        if (t.begin >= L.contentEnd) break;
        const int index = static_cast<int>(k);
        switch (t.kind) {
            case ManuscriptToken::Kind::Text:
                addChars(std::max(t.begin, L.contentBegin), std::min(t.end, L.contentEnd), t.style,
                         index, -1);
                break;
            case ManuscriptToken::Kind::Element:
            case ManuscriptToken::Kind::Value:
                addChars(t.begin, std::min(t.end, L.contentEnd), t.style, index, index);
                break;
            case ManuscriptToken::Kind::Action:
            case ManuscriptToken::Kind::Bookmark:
            case ManuscriptToken::Kind::Note: {
                m.units.push_back({t.raw, t.style});
                UnitInfo info;
                info.pos = t.begin;
                info.token = index;
                info.atom = true;
                m.info.push_back(info);
                break;
            }
            default:
                break;  // Formatzeichen
        }
    }
    return m;
}

// Wie viele Einheiten liegen vor `rawPos`? Das ist die Grenze, an der der
// Cursor steht.
size_t unitIndexAt(const LineModel& m, size_t rawPos) {
    size_t lo = 0, hi = m.info.size();
    while (lo < hi) {
        const size_t mid = (lo + hi) / 2;
        if (m.info[mid].pos < rawPos)
            lo = mid + 1;
        else
            hi = mid;
    }
    return lo;
}

// Zeile zurueck in Quelltext (ohne '\n'). `positions`: Offset jeder Einheit,
// `contentStart`: wo der Inhalt hinter dem Vorspann beginnt.
std::string serializeLine(const LineModel& m, std::vector<size_t>* positions, size_t* contentStart) {
    positions->clear();
    if (m.atomic()) {
        *contentStart = 0;
        return m.atomicRaw;
    }
    std::string out = m.prefix;
    *contentStart = out.size();
    if (m.kind == LineKind::Heading) {
        // Ueberschriften sind schlichter Text - ihr Format kommt aus der Vorlage.
        for (const StyledUnit& u : m.units) {
            positions->push_back(out.size());
            out += u.raw;
        }
    } else {
        std::vector<size_t> p;
        out += serializeUnits(m.units, &p);
        for (size_t v : p) positions->push_back(v + *contentStart);
    }
    ParagraphFormat format = m.format;
    format.align = m.align;
    out += paragraphMarker(format);
    return out;
}

// Offset der Grenze `idx` innerhalb der serialisierten Zeile. Vor einer
// Einheit steht der Cursor hinter der vorigen - so erbt Getipptes das Format
// links davon, wie in Word.
size_t boundaryOffset(const LineModel& m, const std::vector<size_t>& positions, size_t idx,
                      size_t contentStart) {
    if (m.atomic() || m.units.empty()) return contentStart;
    if (idx == 0) return positions[0];
    idx = std::min(idx, m.units.size());
    return positions[idx - 1] + m.units[idx - 1].raw.size();
}

void appendUnits(LineModel& dst, const LineModel& src, size_t from = 0) {
    for (size_t k = from; k < src.units.size(); ++k) {
        dst.units.push_back(src.units[k]);
        dst.info.push_back(k < src.info.size() ? src.info[k] : UnitInfo());
    }
}

bool isSpaceUnit(const StyledUnit& u) { return u.raw == " " || u.raw == "\t"; }

ImU32 hexColor(const std::string& hex, ImU32 fallback) {
    if (hex.size() != 7 || hex[0] != '#') return fallback;
    const unsigned long v = std::strtoul(hex.c_str() + 1, nullptr, 16);
    return IM_COL32((v >> 16) & 0xFF, (v >> 8) & 0xFF, v & 0xFF, 0xFF);
}

float headingPt(int level) {
    switch (level) {
        case 1: return 26.0f;
        case 2: return 20.0f;
        case 3: return 15.0f;
        default: return 13.0f;
    }
}

uint64_t mixHash(uint64_t h, uint64_t v) {
    return h ^ (v + 0x9E3779B97F4A7C15ull + (h << 6) + (h >> 2));
}

std::string g_clipPlain;
std::string g_clipSource;

}  // namespace

// ================================================================= Dokument
const Project& ManuscriptDoc::project() const { return ed_->project; }
const std::string& ManuscriptDoc::text() const { return ed_->project.manuscript; }

void ManuscriptDoc::touched() { ++generation_; }

void ManuscriptDoc::sync(Editor& ed) {
    ed_ = &ed;
    const std::string& t = ed.project.manuscript;
    if (t != lastText_) {
        // Von aussen geaendert: die eigene Geschichte passt nicht mehr dazu.
        lastText_ = t;
        undo_.clear();
        redo_.clear();
        touched();
        for (DocumentView* v : views) {
            v->cursor = std::min(v->cursor, t.size());
            v->anchor = std::min(v->anchor, t.size());
        }
    }

    // Namen, Farben und Aktionstitel stecken im gezeichneten Text - aendern
    // sie sich, muss neu aufgeloest werden.
    uint64_t h = 1469598103934665603ull;
    std::hash<std::string> hs;
    for (const Group& g : ed.project.groups) {
        h = mixHash(h, hs(g.id));
        h = mixHash(h, hs(g.name));
        h = mixHash(h, static_cast<uint64_t>(g.color.x * 255.0f) << 16 |
                           static_cast<uint64_t>(g.color.y * 255.0f) << 8 |
                           static_cast<uint64_t>(g.color.z * 255.0f));
    }
    for (const Element& el : ed.project.elements) {
        h = mixHash(h, hs(el.id));
        h = mixHash(h, hs(el.name));
        h = mixHash(h, hs(el.groupId));
    }
    for (const Action& a : ed.project.actions) {
        h = mixHash(h, hs(a.id));
        h = mixHash(h, hs(a.title));
    }
    if (h != projectSig_) {
        projectSig_ = h;
        touched();
    }
}

const std::vector<ManuscriptToken>& ManuscriptDoc::tokens() {
    if (parsedGeneration_ != generation_) {
        tokens_ = parseManuscript(project(), text());
        lines_ = manuscriptLines(text());
        parsedGeneration_ = generation_;
    }
    return tokens_;
}

const std::vector<ManuscriptLine>& ManuscriptDoc::lines() {
    tokens();
    return lines_;
}

LineModel ManuscriptDoc::model(size_t lineIndex) {
    const std::vector<ManuscriptToken>& toks = tokens();
    return buildModel(text(), toks, lines_, std::min(lineIndex, lines_.size() - 1));
}

void ManuscriptDoc::shiftOtherViews(DocumentView* by, size_t pos, size_t removed, size_t inserted) {
    for (DocumentView* v : views) {
        if (v == by) continue;
        for (size_t* p : {&v->cursor, &v->anchor}) {
            if (*p >= pos + removed)
                *p = *p - removed + inserted;
            else if (*p > pos)
                *p = pos + inserted;
        }
    }
}

void ManuscriptDoc::replace(size_t pos, size_t len, const std::string& insert, DocumentView* by,
                            size_t cursorAfter, size_t anchorAfter, bool typing) {
    std::string& t = ed_->project.manuscript;
    pos = std::min(pos, t.size());
    len = std::min(len, t.size() - pos);
    Edit e;
    e.pos = pos;
    e.removed = t.substr(pos, len);
    e.inserted = insert;

    if (e.removed != e.inserted) {
        const double now = ImGui::GetTime();
        const bool merge = typing && !undo_.empty() && undo_.back().typing &&
                           now - undo_.back().time < 1.5;
        if (merge) {
            undo_.back().edits.push_back(e);
            undo_.back().cursorAfter = cursorAfter;
            undo_.back().anchorAfter = anchorAfter;
            undo_.back().time = now;
        } else {
            Step s;
            s.edits.push_back(e);
            s.cursorBefore = by ? by->cursor : pos;
            s.anchorBefore = by ? by->anchor : pos;
            s.cursorAfter = cursorAfter;
            s.anchorAfter = anchorAfter;
            s.time = now;
            s.typing = typing;
            undo_.push_back(std::move(s));
            if (undo_.size() > 400) undo_.pop_front();
        }
        redo_.clear();

        t.replace(pos, len, insert);
        lastText_ = t;
        touched();
        ed_->markManuscript();
        shiftOtherViews(by, pos, len, insert.size());
    }
    if (by) {
        by->cursor = std::min(cursorAfter, t.size());
        by->anchor = std::min(anchorAfter, t.size());
    }
}

bool ManuscriptDoc::undo(DocumentView* view) {
    if (undo_.empty()) return false;
    Step s = undo_.back();
    undo_.pop_back();
    std::string& t = ed_->project.manuscript;
    for (size_t i = s.edits.size(); i-- > 0;) {
        const Edit& e = s.edits[i];
        t.replace(e.pos, e.inserted.size(), e.removed);
        shiftOtherViews(view, e.pos, e.inserted.size(), e.removed.size());
    }
    lastText_ = t;
    touched();
    ed_->markManuscript();
    if (view) {
        view->cursor = std::min(s.cursorBefore, t.size());
        view->anchor = std::min(s.anchorBefore, t.size());
    }
    redo_.push_back(std::move(s));
    return true;
}

bool ManuscriptDoc::redo(DocumentView* view) {
    if (redo_.empty()) return false;
    Step s = redo_.back();
    redo_.pop_back();
    std::string& t = ed_->project.manuscript;
    for (const Edit& e : s.edits) {
        t.replace(e.pos, e.removed.size(), e.inserted);
        shiftOtherViews(view, e.pos, e.removed.size(), e.inserted.size());
    }
    lastText_ = t;
    touched();
    ed_->markManuscript();
    if (view) {
        view->cursor = std::min(s.cursorAfter, t.size());
        view->anchor = std::min(s.anchorAfter, t.size());
    }
    undo_.push_back(std::move(s));
    return true;
}

// ============================================================== Bearbeiten
namespace docops {
namespace {

std::vector<LineModel> modelsOf(const Project& p, const std::string& source) {
    const std::vector<ManuscriptToken> toks = parseManuscript(p, source);
    const std::vector<ManuscriptLine> lns = manuscriptLines(source);
    std::vector<LineModel> out;
    for (size_t i = 0; i < lns.size(); ++i) out.push_back(buildModel(source, toks, lns, i));
    return out;
}

// Format fuer neu eingefuegten Text an `pos`.
TextStyle styleForInsert(ManuscriptDoc& doc, size_t pos, bool replacing) {
    const std::vector<ManuscriptLine>& lines = doc.lines();
    const size_t li = lineIndexAt(lines, pos);
    if (isAtomicKind(lines[li].kind) || lines[li].kind == LineKind::Heading) return TextStyle();
    const LineModel m = doc.model(li);
    const size_t idx = unitIndexAt(m, pos);
    if (replacing && idx < m.units.size()) return m.units[idx].style;
    if (idx > 0) return m.units[idx - 1].style;
    if (idx < m.units.size()) return m.units[idx].style;
    return TextStyle();
}

// Das zentrale Werkzeug: Bereich [A, B) durch Zeilen ersetzen. Die erste
// Fragmentzeile verschmilzt mit dem Rest links von A, die letzte mit dem Rest
// rechts von B - genau wie beim Tippen, Einfuegen und Loeschen in Word.
void replaceRange(ManuscriptDoc& doc, DocumentView& view, size_t A, size_t B,
                  std::vector<LineModel> frag, bool typing) {
    if (frag.empty()) frag.emplace_back();
    if (B < A) std::swap(A, B);
    const std::vector<ManuscriptLine>& lines = doc.lines();
    size_t la = lineIndexAt(lines, A);
    size_t lb = lineIndexAt(lines, B);

    // Cursor steht vor einer Zeit- oder Trennzeile: Neues kommt als eigene Zeile davor.
    if (A == B && isAtomicKind(lines[la].kind)) {
        std::string ins;
        size_t caret = 0;
        for (size_t i = 0; i < frag.size(); ++i) {
            std::vector<size_t> pos;
            size_t cs = 0;
            const std::string s = serializeLine(frag[i], &pos, &cs);
            if (i + 1 == frag.size())
                caret = ins.size() + boundaryOffset(frag[i], pos, frag[i].units.size(), cs);
            ins += s;
            ins += '\n';
        }
        const size_t at = lines[la].begin;
        doc.replace(at, 0, ins, &view, at + caret, at + caret, typing);
        return;
    }
    // Endet der Bereich am Anfang einer Zeit-/Trennzeile, bleibt die unberuehrt.
    if (lb > la && isAtomicKind(lines[lb].kind) && B == lines[lb].begin) {
        --lb;
        B = lines[lb].end;
    }

    const size_t from = lines[la].begin;
    const size_t to = lines[lb].end;
    LineModel first = doc.model(la);
    LineModel last = lb == la ? first : doc.model(lb);
    const size_t ia = first.atomic() ? 0 : unitIndexAt(first, A);
    const size_t ib = last.atomic() ? last.units.size() : unitIndexAt(last, B);

    LineModel head = first.atomic() ? LineModel() : first;
    head.units.resize(std::min(ia, head.units.size()));
    head.info.resize(head.units.size());

    std::vector<LineModel> out;
    size_t caretLine = 0, caretIdx = 0;
    if (frag.size() == 1) {
        appendUnits(head, frag[0]);
        caretIdx = head.units.size();
        if (!last.atomic()) appendUnits(head, last, ib);
        out.push_back(std::move(head));
    } else {
        appendUnits(head, frag[0]);
        out.push_back(std::move(head));
        for (size_t i = 1; i + 1 < frag.size(); ++i) out.push_back(frag[i]);
        LineModel tail = frag.back();
        caretIdx = tail.units.size();
        if (!last.atomic()) appendUnits(tail, last, ib);
        out.push_back(std::move(tail));
        caretLine = out.size() - 1;
    }

    std::string ins;
    size_t caret = 0;
    for (size_t i = 0; i < out.size(); ++i) {
        if (i) ins += '\n';
        std::vector<size_t> pos;
        size_t cs = 0;
        const std::string s = serializeLine(out[i], &pos, &cs);
        if (i == caretLine) caret = ins.size() + boundaryOffset(out[i], pos, caretIdx, cs);
        ins += s;
    }
    doc.replace(from, to - from, ins, &view, from + caret, from + caret, typing);
}

// Zeilen la..lb neu schreiben. `change` bekommt jede Zeile samt dem Teil, der
// in der Auswahl liegt. Cursor und Anker behalten ihre Einheit.
void rewriteLines(ManuscriptDoc& doc, DocumentView& view, size_t la, size_t lb,
                  const std::function<void(LineModel&, size_t, size_t)>& change) {
    const std::vector<ManuscriptLine> lines = doc.lines();
    const size_t sb = view.selBegin(), se = view.selEnd();
    const size_t lsb = lineIndexAt(lines, sb), lse = lineIndexAt(lines, se);
    const size_t lc = lineIndexAt(lines, view.cursor), lanc = lineIndexAt(lines, view.anchor);
    const size_t from = lines[la].begin, to = lines[lb].end;

    std::string out;
    size_t newC = view.cursor, newA = view.anchor;
    for (size_t li = la; li <= lb; ++li) {
        LineModel m = doc.model(li);
        size_t ic = kNone, iaHere = kNone;
        if (!m.atomic()) {
            if (li == lc) ic = unitIndexAt(m, view.cursor);
            if (li == lanc) iaHere = unitIndexAt(m, view.anchor);
            const size_t ia = li == lsb ? unitIndexAt(m, sb) : 0;
            const size_t ib = li == lse ? unitIndexAt(m, se) : m.units.size();
            change(m, ia, std::max(ia, ib));
        }
        std::vector<size_t> pos;
        size_t cs = 0;
        if (li > la) out += '\n';
        const std::string s = serializeLine(m, &pos, &cs);
        const size_t base = from + out.size();
        if (li == lc) newC = ic == kNone ? base : base + boundaryOffset(m, pos, ic, cs);
        if (li == lanc) newA = iaHere == kNone ? base : base + boundaryOffset(m, pos, iaHere, cs);
        out += s;
    }
    doc.replace(from, to - from, out, &view, newC, newA);
}

void affectedLines(ManuscriptDoc& doc, DocumentView& view, size_t* la, size_t* lb) {
    const std::vector<ManuscriptLine>& lines = doc.lines();
    *la = lineIndexAt(lines, view.selBegin());
    *lb = lineIndexAt(lines, view.selEnd());
    // Endet die Auswahl ganz vorne in einer Zeile, gehoert die nicht dazu.
    if (*lb > *la && view.selEnd() <= lines[*lb].contentBegin) --*lb;
}

// Grenze `idx` einer Zeile als Position im aktuellen Text.
size_t boundaryRaw(ManuscriptDoc& doc, size_t li, const LineModel& m, size_t idx) {
    const ManuscriptLine& L = doc.lines()[li];
    if (m.atomic()) return L.begin;
    if (m.units.empty()) return L.contentBegin;
    if (idx == 0) return m.info[0].pos;
    idx = std::min(idx, m.units.size());
    return m.info[idx - 1].pos + m.units[idx - 1].raw.size();
}

}  // namespace

void typeText(ManuscriptDoc& doc, DocumentView& view, const std::string& text, bool typing) {
    if (text.empty()) return;
    const size_t A = view.selBegin(), B = view.selEnd();
    const TextStyle style =
        view.pendingActive ? view.pending : styleForInsert(doc, A, B != A);

    std::string body = text;
    // Direkt hinter einer Aktionsmarke wuerde ein Buchstabe zur ID gehoeren.
    {
        const std::vector<ManuscriptLine>& lines = doc.lines();
        const size_t li = lineIndexAt(lines, A);
        if (!isAtomicKind(lines[li].kind)) {
            const LineModel m = doc.model(li);
            const size_t idx = unitIndexAt(m, A);
            if (idx > 0 && m.info[idx - 1].atom && m.info[idx - 1].token >= 0 &&
                doc.tokens()[static_cast<size_t>(m.info[idx - 1].token)].kind ==
                    ManuscriptToken::Kind::Action &&
                isNameStart(static_cast<unsigned char>(body[0])))
                body.insert(body.begin(), ' ');
        }
    }

    std::vector<LineModel> frag(1);
    size_t i = 0;
    while (i < body.size()) {
        const unsigned char ch = static_cast<unsigned char>(body[i]);
        if (ch == '\r') {
            ++i;
            continue;
        }
        if (ch == '\n') {
            frag.emplace_back();
            ++i;
            continue;
        }
        const size_t n = std::min(utf8Len(ch), body.size() - i);
        frag.back().units.push_back({body.substr(i, n), style});
        frag.back().info.push_back(UnitInfo());
        i += n;
    }
    replaceRange(doc, view, A, B, frag, typing && frag.size() == 1);
    view.pendingActive = false;
}

void insertSource(ManuscriptDoc& doc, DocumentView& view, const std::string& source) {
    if (source.empty()) return;
    std::vector<LineModel> frag = modelsOf(doc.project(), source);
    // Eine einzelne Marke uebernimmt das Format der Umgebung (fett bleibt fett).
    if (frag.size() == 1) {
        const TextStyle style = view.pendingActive
                                    ? view.pending
                                    : styleForInsert(doc, view.selBegin(), view.hasSelection());
        for (StyledUnit& u : frag[0].units) {
            if (u.style.plain()) u.style = style;
        }
    }
    replaceRange(doc, view, view.selBegin(), view.selEnd(), frag, false);
    view.pendingActive = false;
}

void insertReference(ManuscriptDoc& doc, DocumentView& view, const std::string& reference) {
    const std::vector<ManuscriptLine>& lines = doc.lines();
    const size_t end = view.selEnd();
    const size_t li = lineIndexAt(lines, end);
    std::string source = reference;
    if (!isAtomicKind(lines[li].kind)) {
        const LineModel m = doc.model(li);
        const size_t idx = unitIndexAt(m, end);
        if (idx < m.units.size() && !m.info[idx].atom &&
            isNameStart(static_cast<unsigned char>(m.units[idx].raw[0])))
            source += " ";
    }
    insertSource(doc, view, source);
}

void insertOwnLine(ManuscriptDoc& doc, DocumentView& view, const std::string& line) {
    if (view.hasSelection()) view.setCaret(view.selEnd());
    const std::vector<ManuscriptLine> lines = doc.lines();
    const size_t li = lineIndexAt(lines, view.cursor);
    const ManuscriptLine& L = lines[li];
    const LineModel m = doc.model(li);
    const std::string& text = doc.text();

    if (!m.atomic() && m.units.empty() && m.kind == LineKind::Body && m.align == LineAlign::Left) {
        // leere Zeile: sie wird zur neuen Zeile, darunter geht es weiter
        const std::string ins = line + "\n";
        const size_t caret = L.begin + ins.size();
        doc.replace(L.begin, L.end - L.begin, ins, &view, caret, caret);
        return;
    }
    const bool atStart = m.atomic() || unitIndexAt(m, view.cursor) == 0;
    if (atStart) {
        const std::string ins = line + "\n";
        const size_t caret = view.cursor + ins.size();
        doc.replace(L.begin, 0, ins, &view, caret, caret);
        return;
    }
    if (L.end >= text.size()) {
        const std::string ins = "\n" + line + "\n";
        const size_t caret = L.end + ins.size();
        doc.replace(L.end, 0, ins, &view, caret, caret);
    } else {
        const std::string ins = "\n" + line;
        const size_t caret = L.end + ins.size() + 1;  // Anfang der naechsten Zeile
        doc.replace(L.end, 0, ins, &view, caret, caret);
    }
}

void newParagraph(ManuscriptDoc& doc, DocumentView& view) {
    if (view.hasSelection()) deleteSelection(doc, view);
    const std::vector<ManuscriptLine> lines = doc.lines();
    const size_t A = view.cursor;
    const size_t li = lineIndexAt(lines, A);
    const ManuscriptLine& L = lines[li];

    if (isAtomicKind(L.kind)) {
        doc.replace(L.begin, 0, "\n", &view, A + 1, A + 1);
        return;
    }
    const LineModel m = doc.model(li);
    const size_t idx = unitIndexAt(m, A);

    // Leerer Listenpunkt + Enter beendet die Liste.
    if ((m.kind == LineKind::Bullet || m.kind == LineKind::Numbered) && m.units.empty()) {
        view.setCaret(A);
        setLineKind(doc, view, LineKind::Body, 0);
        return;
    }
    // Ganz vorne in einer Zeile mit Inhalt: leere Zeile darueber.
    if (idx == 0 && !m.units.empty()) {
        doc.replace(L.begin, 0, "\n", &view, A + 1, A + 1);
        return;
    }

    LineModel next;
    next.kind = m.kind == LineKind::Heading ? LineKind::Body : m.kind;
    next.align = m.kind == LineKind::Heading ? LineAlign::Left : m.align;
    // Einzuege und Tabstopps gehen wie in Word auf den neuen Absatz ueber.
    if (m.kind != LineKind::Heading) next.format = m.format;
    next.prefix = linePrefix(next.kind, 0);
    std::vector<LineModel> frag(2);
    frag[1] = next;
    // Am Zeilenende geht das Zeichenformat wie in Word in die neue Zeile mit.
    const TextStyle carried = view.pendingActive ? view.pending
                              : idx > 0 && idx == m.units.size() && m.kind != LineKind::Heading
                                  ? m.units[idx - 1].style
                                  : TextStyle();
    replaceRange(doc, view, A, A, frag, false);
    if (!carried.plain()) {
        view.pending = carried;
        view.pendingActive = true;
    }
}

void deleteSelection(ManuscriptDoc& doc, DocumentView& view) {
    if (!view.hasSelection()) return;
    replaceRange(doc, view, view.selBegin(), view.selEnd(), {LineModel()}, false);
}

void backspace(ManuscriptDoc& doc, DocumentView& view, bool word) {
    if (view.hasSelection()) {
        deleteSelection(doc, view);
        return;
    }
    const std::vector<ManuscriptLine> lines = doc.lines();
    const size_t A = view.cursor;
    const size_t li = lineIndexAt(lines, A);
    const ManuscriptLine& L = lines[li];
    const LineModel m = doc.model(li);
    const size_t idx = m.atomic() ? 0 : unitIndexAt(m, A);

    if (idx == 0) {
        // Erst die Aufzaehlung bzw. Ueberschrift loesen, dann verbinden.
        if (!m.atomic() && m.kind != LineKind::Body) {
            setLineKind(doc, view, LineKind::Body, 0);
            return;
        }
        if (li == 0) return;
        const ManuscriptLine& prev = lines[li - 1];
        if (isAtomicKind(prev.kind)) {
            const size_t removed = L.begin - prev.begin;
            doc.replace(prev.begin, removed, "", &view, A - removed, A - removed, true);
            return;
        }
        if (m.atomic()) {
            // Zeit-/Trennzeile: der Zeilenumbruch davor verschwindet nicht,
            // sonst klebte die Marke am Text - Cursor springt nur hoch.
            view.setCaret(prev.end);
            return;
        }
        replaceRange(doc, view, prev.end, A, {LineModel()}, true);
        return;
    }
    size_t k = idx;
    if (word) {
        while (k > 0 && isSpaceUnit(m.units[k - 1])) --k;
        while (k > 0 && !isSpaceUnit(m.units[k - 1])) --k;
    } else {
        --k;
    }
    replaceRange(doc, view, boundaryRaw(doc, li, m, k), A, {LineModel()}, true);
}

void deleteForward(ManuscriptDoc& doc, DocumentView& view, bool word) {
    if (view.hasSelection()) {
        deleteSelection(doc, view);
        return;
    }
    const std::vector<ManuscriptLine> lines = doc.lines();
    const size_t A = view.cursor;
    const size_t li = lineIndexAt(lines, A);
    const ManuscriptLine& L = lines[li];
    const std::string& text = doc.text();

    if (isAtomicKind(L.kind)) {
        // Zeit- oder Trennzeile als Ganzes entfernen
        const size_t end = L.end < text.size() ? L.end + 1 : L.end;
        doc.replace(L.begin, end - L.begin, "", &view, L.begin, L.begin);
        return;
    }
    const LineModel m = doc.model(li);
    const size_t idx = unitIndexAt(m, A);
    if (idx >= m.units.size()) {
        if (li + 1 >= lines.size()) return;
        const ManuscriptLine& next = lines[li + 1];
        if (isAtomicKind(next.kind)) {
            const size_t end = next.end < text.size() ? next.end + 1 : next.end;
            doc.replace(next.begin, end - next.begin, "", &view, A, A, true);
            return;
        }
        replaceRange(doc, view, A, next.contentBegin, {LineModel()}, true);
        return;
    }
    size_t k = idx;
    if (word) {
        while (k < m.units.size() && !isSpaceUnit(m.units[k])) ++k;
        while (k < m.units.size() && isSpaceUnit(m.units[k])) ++k;
    } else {
        ++k;
    }
    const size_t end = k >= m.units.size() ? L.contentEnd : m.info[k].pos;
    const size_t begin = idx == 0 ? m.info[0].pos : A;
    replaceRange(doc, view, begin, std::max(begin, end), {LineModel()}, true);
}

TextStyle caretStyle(ManuscriptDoc& doc, DocumentView& view) {
    if (view.pendingActive) return view.pending;
    return styleForInsert(doc, view.cursor, false);
}

// Wort, in dessen Mitte der Cursor steht (ohne Auswahl, ohne vorgemerkte
// Formatierung) - als Einheitenbereich [wa, wb) der Zeile li.
bool caretWord(ManuscriptDoc& doc, DocumentView& view, size_t* li, size_t* wa, size_t* wb) {
    if (view.hasSelection() || view.pendingActive) return false;
    *li = lineIndexAt(doc.lines(), view.cursor);
    const LineModel m = doc.model(*li);
    if (m.atomic()) return false;
    auto wordUnit = [&](size_t k) {
        const unsigned char c = m.units[k].raw.empty() ? ' ' : m.units[k].raw[0];
        return std::isalnum(c) || c >= 0x80 || c == '_';
    };
    const size_t idx = unitIndexAt(m, view.cursor);
    if (idx == 0 || idx >= m.units.size() || !wordUnit(idx - 1) || !wordUnit(idx)) return false;
    *wa = *wb = idx;
    while (*wa > 0 && wordUnit(*wa - 1)) --*wa;
    while (*wb < m.units.size() && wordUnit(*wb)) ++*wb;
    return true;
}

void applyStyle(ManuscriptDoc& doc, DocumentView& view,
                const std::function<void(TextStyle&)>& change) {
    if (!view.hasSelection()) {
        // Wie in Word: Steht der Cursor mitten in einem Wort, gilt die
        // Formatierung dem ganzen Wort.
        size_t li = 0, wa = 0, wb = 0;
        if (caretWord(doc, view, &li, &wa, &wb)) {
            rewriteLines(doc, view, li, li, [&](LineModel& m, size_t, size_t) {
                for (size_t k = wa; k < wb; ++k) change(m.units[k].style);
            });
            return;
        }
        if (!view.pendingActive) {
            view.pending = caretStyle(doc, view);
            view.pendingActive = true;
        }
        change(view.pending);
        return;
    }
    const std::vector<ManuscriptLine>& lines = doc.lines();
    const size_t la = lineIndexAt(lines, view.selBegin());
    const size_t lb = lineIndexAt(lines, view.selEnd());
    rewriteLines(doc, view, la, lb, [&](LineModel& m, size_t ia, size_t ib) {
        std::vector<bool> mark(m.units.size(), false);
        for (size_t k = ia; k < ib && k < m.units.size(); ++k) {
            mark[k] = true;
            // Elementmarken nur als Ganzes formatieren - sonst zerfiele "@Alice".
            if (m.info[k].group >= 0) {
                for (size_t j = 0; j < m.units.size(); ++j) {
                    if (m.info[j].group == m.info[k].group) mark[j] = true;
                }
            }
        }
        for (size_t k = 0; k < m.units.size(); ++k) {
            if (mark[k]) change(m.units[k].style);
        }
    });
}

bool selectionHas(ManuscriptDoc& doc, DocumentView& view,
                  const std::function<bool(const TextStyle&)>& test) {
    size_t wl = 0, wa = 0, wb = 0;
    if (caretWord(doc, view, &wl, &wa, &wb)) {
        const LineModel m = doc.model(wl);
        for (size_t k = wa; k < wb; ++k) {
            if (!test(m.units[k].style)) return false;
        }
        return true;
    }
    if (!view.hasSelection()) return test(caretStyle(doc, view));
    const std::vector<ManuscriptLine> lines = doc.lines();
    const size_t la = lineIndexAt(lines, view.selBegin());
    const size_t lb = lineIndexAt(lines, view.selEnd());
    size_t seen = 0;
    for (size_t li = la; li <= lb; ++li) {
        const LineModel m = doc.model(li);
        if (m.atomic() || m.kind == LineKind::Heading) continue;
        const size_t ia = li == la ? unitIndexAt(m, view.selBegin()) : 0;
        const size_t ib = li == lb ? unitIndexAt(m, view.selEnd()) : m.units.size();
        for (size_t k = ia; k < ib; ++k) {
            if (!test(m.units[k].style)) return false;
            ++seen;
        }
    }
    return seen > 0;
}

void setLineKind(ManuscriptDoc& doc, DocumentView& view, LineKind kind, int level) {
    size_t la = 0, lb = 0;
    affectedLines(doc, view, &la, &lb);
    rewriteLines(doc, view, la, lb, [&](LineModel& m, size_t, size_t) {
        if (m.kind == LineKind::Heading && kind != LineKind::Heading) {
            // Ueberschriftentext bleibt Text - ohne Format
            for (StyledUnit& u : m.units) u.style = TextStyle();
        }
        m.kind = kind;
        m.level = kind == LineKind::Heading ? level : 0;
        m.prefix = linePrefix(kind, m.level);
    });
}

void setAlign(ManuscriptDoc& doc, DocumentView& view, LineAlign align) {
    size_t la = 0, lb = 0;
    affectedLines(doc, view, &la, &lb);
    rewriteLines(doc, view, la, lb, [&](LineModel& m, size_t, size_t) { m.align = align; });
}

void setParagraphFormat(ManuscriptDoc& doc, DocumentView& view,
                        const std::function<void(ParagraphFormat&)>& change) {
    size_t la = 0, lb = 0;
    affectedLines(doc, view, &la, &lb);
    rewriteLines(doc, view, la, lb, [&](LineModel& m, size_t, size_t) {
        ParagraphFormat f = m.format;
        f.align = m.align;
        change(f);
        std::sort(f.tabs.begin(), f.tabs.end());
        m.format = f;
        m.align = f.align;
    });
}

const ManuscriptLine* caretLine(ManuscriptDoc& doc, DocumentView& view) {
    const std::vector<ManuscriptLine>& lines = doc.lines();
    if (lines.empty()) return nullptr;
    return &lines[lineIndexAt(lines, view.cursor)];
}

std::string selectedPlainText(ManuscriptDoc& doc, DocumentView& view) {
    if (!view.hasSelection()) return std::string();
    const std::vector<ManuscriptLine> lines = doc.lines();
    const size_t la = lineIndexAt(lines, view.selBegin());
    const size_t lb = lineIndexAt(lines, view.selEnd());
    std::string out;
    for (size_t li = la; li <= lb; ++li) {
        if (li > la) out += '\n';
        const LineModel m = doc.model(li);
        if (m.atomic()) continue;
        const size_t ia = li == la ? unitIndexAt(m, view.selBegin()) : 0;
        const size_t ib = li == lb ? unitIndexAt(m, view.selEnd()) : m.units.size();
        for (size_t k = ia; k < ib; ++k) {
            if (m.info[k].atom) continue;
            out += m.units[k].raw;
        }
    }
    return out;
}

// Auswahl als Quelltext - fuer Einfuegen im Manuskript samt Format.
std::string selectedSource(ManuscriptDoc& doc, DocumentView& view) {
    const std::vector<ManuscriptLine> lines = doc.lines();
    const size_t la = lineIndexAt(lines, view.selBegin());
    const size_t lb = lineIndexAt(lines, view.selEnd());
    std::string out;
    for (size_t li = la; li <= lb; ++li) {
        if (li > la) out += '\n';
        LineModel m = doc.model(li);
        if (m.atomic()) {
            if (li != la || view.selBegin() <= lines[li].begin) out += m.atomicRaw;
            continue;
        }
        const size_t ia = li == la ? unitIndexAt(m, view.selBegin()) : 0;
        const size_t ib = li == lb ? unitIndexAt(m, view.selEnd()) : m.units.size();
        LineModel part = m;
        part.units.assign(m.units.begin() + static_cast<long>(ia),
                          m.units.begin() + static_cast<long>(std::max(ia, ib)));
        part.info.assign(m.info.begin() + static_cast<long>(ia),
                         m.info.begin() + static_cast<long>(std::max(ia, ib)));
        std::vector<size_t> pos;
        size_t cs = 0;
        out += serializeLine(part, &pos, &cs);
    }
    return out;
}

void copy(ManuscriptDoc& doc, DocumentView& view) {
    if (!view.hasSelection()) return;
    g_clipPlain = selectedPlainText(doc, view);
    g_clipSource = selectedSource(doc, view);
    ImGui::SetClipboardText(g_clipPlain.c_str());
}

void cut(ManuscriptDoc& doc, DocumentView& view) {
    if (!view.hasSelection()) return;
    copy(doc, view);
    deleteSelection(doc, view);
}

void paste(ManuscriptDoc& doc, DocumentView& view) {
    const char* clip = ImGui::GetClipboardText();
    if (!clip || !*clip) return;
    const std::string text = clip;
    // Kommt der Text aus diesem Manuskript, wird er mitsamt Format eingefuegt.
    if (!g_clipSource.empty() && text == g_clipPlain) {
        std::vector<LineModel> frag = modelsOf(doc.project(), g_clipSource);
        replaceRange(doc, view, view.selBegin(), view.selEnd(), frag, false);
        return;
    }
    typeText(doc, view, text, false);
}

bool findSpellingError(ManuscriptDoc& doc, size_t from, size_t* begin, size_t* end) {
    if (!spell::available()) return false;
    const std::vector<ManuscriptLine> lines = doc.lines();
    if (lines.empty()) return false;
    const size_t start = lineIndexAt(lines, from);
    for (size_t step = 0; step <= lines.size(); ++step) {
        const size_t li = (start + step) % lines.size();
        if (isAtomicKind(lines[li].kind)) continue;
        const LineModel m = doc.model(li);
        std::string plain;
        std::vector<size_t> pos;
        for (size_t k = 0; k < m.units.size(); ++k) {
            if (m.info[k].atom || m.info[k].group >= 0) {
                plain += ' ';
                pos.push_back(m.info[k].pos);
                continue;
            }
            for (size_t j = 0; j < m.units[k].raw.size(); ++j) {
                plain += m.units[k].raw[j];
                pos.push_back(m.info[k].pos + j);
            }
        }
        for (const spell::Issue& issue : spell::check(plain)) {
            if (issue.end == 0 || issue.end > pos.size()) continue;
            const size_t a = pos[issue.begin], b = pos[issue.end - 1] + 1;
            // In der Startzeile nur, was hinter dem Cursor liegt (beim Umlauf alles).
            if (step == 0 && a < from) continue;
            *begin = a;
            *end = b;
            return true;
        }
    }
    return false;
}

void replaceRaw(ManuscriptDoc& doc, DocumentView& view, size_t begin, size_t end,
                const std::string& text) {
    const size_t caret = begin + text.size();
    doc.replace(begin, end - begin, text, &view, caret, caret);
}

}  // namespace docops

// ================================================================== Ansicht
void DocumentView::setCaret(size_t pos, bool keepAnchor) {
    cursor = pos;
    if (!keepAnchor) anchor = pos;
    pendingActive = false;
    scrollToCaret_ = true;
    blinkStart_ = ImGui::GetTime();
}

void DocumentView::select(size_t begin, size_t end) {
    anchor = begin;
    cursor = end;
    pendingActive = false;
    scrollToCaret_ = true;
    blinkStart_ = ImGui::GetTime();
}

void DocumentView::moveCaret(size_t pos, bool extend) {
    cursor = pos;
    if (!extend) anchor = pos;
    pendingActive = false;
    scrollToCaret_ = true;
    blinkStart_ = ImGui::GetTime();
}

std::vector<std::pair<size_t, size_t>> DocumentView::visibleSpellIssues() const {
    std::vector<std::pair<size_t, size_t>> out;
    const LineLayout* last = nullptr;
    for (const Row& row : rows_) {
        if (!row.layout || row.layout == last || row.layout->spellGeneration != spell::generation()) continue;
        last = row.layout;
        for (const auto& issue : row.layout->spellIssues)
            out.push_back({row.lineBegin + issue.first, row.lineBegin + issue.second});
    }
    return out;
}

float DocumentView::zoomForPageWidth(const DocOptions& options) const {
    const float w = options.pageWidthCm * kPxPerCm;
    return std::clamp((viewWidth_ - 40.0f) / w, 0.3f, 4.0f);
}

float DocumentView::zoomForWholePage(const DocOptions& options) const {
    const float w = options.pageWidthCm * kPxPerCm;
    const float h = options.pageHeightCm * kPxPerCm;
    return std::clamp(std::min((viewWidth_ - 40.0f) / w, (viewHeight_ - 36.0f) / h), 0.3f, 4.0f);
}

// ------------------------------------------------------------------ Layout
namespace {

struct LayUnit {
    size_t pos = 0, len = 0;  // relativ zum Zeilenanfang
    float adv = 0.0f;
    ImFont* font = nullptr;
    float px = 0.0f;
    float rise = 0.0f;
    float ascent = 0.0f, descent = 0.0f;
    uint8_t kind = 0;  // 0 Zeichen, 1 Leerraum, 2 Marke
    uint8_t cls = 2;
    uint8_t atomKind = 0;
    int token = -1;  // relativ zur ersten Marke der Zeile
    int label = -1;
    int style = -1;
    bool chip = false;
    bool tab = false;  // Tabulator: Breite haengt von der Position ab
};

uint8_t classOf(unsigned int cp) {
    if (cp == ' ' || cp == '\t') return 1;
    if (cp >= 0x80 || std::isalnum(static_cast<int>(cp)) || cp == '_' || cp == '@') return 2;
    return 3;
}

const TextStyle& plainStyle() {
    static const TextStyle s;
    return s;
}

uint64_t hashStyle(const TextStyle& s) {
    uint64_t h = (s.bold ? 1u : 0u) | (s.italic ? 2u : 0u) | (s.underline ? 4u : 0u) | (s.strike ? 8u : 0u) |
                 (s.superscript ? 16u : 0u) | (s.subscript ? 32u : 0u);
    std::hash<std::string> hs;
    h = mixHash(h, hs(s.color));
    h = mixHash(h, hs(s.background));
    h = mixHash(h, hs(s.font));
    return mixHash(h, static_cast<uint64_t>(s.size * 100.0f));
}

}  // namespace

// Eine Textzeile setzen. Alle Positionen sind relativ (Quelltext zum
// Zeilenanfang, x zum linken Rand, Marken zur ersten Marke der Zeile) - so
// kann das Ergebnis wiederverwendet werden, solange die Zeile gleich bleibt.
void DocumentView::buildLine(ManuscriptDoc& doc, const DocOptions& opt, size_t li, float contentW,
                             LineLayout& out) {
    const std::string& text = doc.text();
    const std::vector<ManuscriptToken>& toks = doc.tokens();
    const std::vector<ManuscriptLine>& lines = doc.lines();
    const Project& project = doc.project();
    const ManuscriptLine& L = lines[li];
    const size_t base = firstTokenAfter(toks, L.contentBegin);

    const float z = opt.zoom;
    const float basePt = opt.fontPt;
    const float basePx = basePt * kPxPerPt * z;
    ImFont* baseFont = theme::familyFont(opt.font, false, false);
    ImFont* uiFont = theme::fonts().regular ? theme::fonts().regular : ImGui::GetFont();
    auto metrics = [](ImFont* f, float px, float* asc, float* desc) {
        ImFontBaked* b = f->GetFontBaked(px);
        *asc = b->Ascent;
        *desc = -b->Descent;
    };
    auto styleIndex = [&](const TextStyle& st) {
        for (size_t i = 0; i < out.styles.size(); ++i) {
            if (out.styles[i] == st) return static_cast<int>(i);
        }
        out.styles.push_back(st);
        return static_cast<int>(out.styles.size() - 1);
    };
    auto addLabel = [&](const std::string& s) {
        out.labels.push_back(s);
        return static_cast<int>(out.labels.size() - 1);
    };

    const LineModel m = buildModel(text, toks, lines, li);
    const bool heading = m.kind == LineKind::Heading;
    const float linePt = heading ? headingPt(m.level) : basePt;

    // Sichtbarer Text fuer die Rechtschreibung: Marken werden zu Leerzeichen.
    for (size_t k = 0; k < m.units.size(); ++k) {
        const uint32_t rel = static_cast<uint32_t>(m.info[k].pos - L.begin);
        if (m.info[k].atom || m.info[k].group >= 0) {
            if (out.plain.empty() || out.plain.back() != ' ') {
                out.plain += ' ';
                out.plainPos.push_back(rel);
            }
            continue;
        }
        const std::string& raw = m.units[k].raw;
        for (size_t j = 0; j < raw.size(); ++j) {
            out.plain += raw[j];
            out.plainPos.push_back(rel + static_cast<uint32_t>(j));
        }
    }
    const float linePx = linePt * kPxPerPt * z;
    ImFont* lineFont = theme::familyFont(opt.font, heading, false);
    out.spaceBefore = heading ? linePx * (m.level <= 1 ? 0.3f : 0.75f) : 0.0f;
    out.spaceAfter = heading ? linePx * 0.35f : 0.0f;

    std::vector<LayUnit> u;
    u.reserve(m.units.size());
    for (size_t k = 0; k < m.units.size(); ++k) {
        const UnitInfo& info = m.info[k];
        const StyledUnit& su = m.units[k];
        const ManuscriptToken* tok = info.token >= 0 ? &toks[static_cast<size_t>(info.token)] : nullptr;
        const TextStyle& st = su.style;

        LayUnit lu;
        lu.pos = info.pos - L.begin;
        lu.len = su.raw.size();
        lu.token = info.token >= 0 ? info.token - static_cast<int>(base) : -1;
        lu.style = styleIndex(heading ? TextStyle() : st);
        const float pt = heading ? linePt : (st.size > 0.0f ? st.size : basePt);
        lu.px = pt * kPxPerPt * z;
        lu.font = heading ? lineFont
                          : theme::familyFont(st.font.empty() ? opt.font : st.font, st.bold, st.italic);
        if (!heading && (st.superscript || st.subscript)) {
            lu.rise = st.superscript ? lu.px * 0.33f : -lu.px * 0.12f;
            lu.px *= 0.65f;
        }

        // Lesemodus: eine Elementmarke wird zu ihrem Wert
        if (opt.readMode && info.group >= 0 && tok) {
            std::string label;
            if (tok->kind == ManuscriptToken::Kind::Element) {
                label = tok->resolved ? project.displayName(tok->targetId) : tok->raw;
            } else {
                const std::string value =
                    tok->resolved ? project.valueAt(tok->targetId, tok->field, tok->time) : std::string();
                label = !tok->resolved ? tok->raw : value.empty() ? project.displayName(tok->targetId) : value;
            }
            size_t j = k;
            size_t len = 0;
            while (j < m.units.size() && m.info[j].group == info.group) {
                len += m.units[j].raw.size();
                ++j;
            }
            lu.len = len;
            lu.kind = 2;
            lu.atomKind = 3;
            lu.cls = 2;
            lu.label = addLabel(label);
            lu.adv = lu.font->CalcTextSizeA(lu.px, FLT_MAX, 0.0f, label.c_str()).x;
            metrics(lu.font, lu.px, &lu.ascent, &lu.descent);
            u.push_back(lu);
            k = j - 1;
            continue;
        }

        if (info.atom && tok) {
            if (tok->kind == ManuscriptToken::Kind::Action) {
                if (opt.readMode && !opt.showMarks) continue;
                const Action* a = project.action(tok->targetId);
                const std::string label = std::string("\xE2\x86\x92 ") + (a ? a->title : tok->raw);
                lu.atomKind = 0;
                lu.label = addLabel(label);
                lu.font = uiFont;
                lu.px = lu.px * 0.8f;
                lu.adv = uiFont->CalcTextSizeA(lu.px, FLT_MAX, 0.0f, label.c_str()).x + lu.px * 0.8f;
            } else {
                if (opt.readMode) continue;  // Lesezeichen und Kommentare nur beim Schreiben
                lu.atomKind = tok->kind == ManuscriptToken::Kind::Bookmark ? 1 : 2;
                lu.adv = lu.px * (lu.atomKind == 1 ? 0.8f : 1.05f);
            }
            lu.kind = 2;
            lu.cls = 3;
            metrics(lu.font, lu.px, &lu.ascent, &lu.descent);
            u.push_back(lu);
            continue;
        }

        unsigned int cp = 0;
        ImTextCharFromUtf8(&cp, su.raw.c_str(), su.raw.c_str() + su.raw.size());
        lu.cls = classOf(cp);
        lu.chip = !opt.readMode && info.group >= 0;
        if (cp == '\t') {
            lu.kind = 1;
            lu.tab = true;
            lu.adv = 1.25f * kPxPerCm * z;  // wird beim Umbruch nach den Tabstopps berechnet
        } else {
            lu.kind = cp == ' ' ? 1 : 0;
            lu.adv = lu.font->GetFontBaked(lu.px)->GetCharAdvance(static_cast<ImWchar>(cp));
        }
        metrics(lu.font, lu.px, &lu.ascent, &lu.descent);
        u.push_back(lu);
    }

    // --------------------------------------------------------- Umbruch
    // Einzuege aus dem Lineal: linker Einzug, Erstzeileneinzug, rechter Einzug.
    const bool list = m.kind == LineKind::Bullet || m.kind == LineKind::Numbered;
    const float cmPx = kPxPerCm * z;
    const ParagraphFormat& pf = L.format;
    const float listIndent = list ? 0.63f * cmPx : 0.0f;
    const float leftPx = pf.left * cmPx, rightPx = pf.right * cmPx, firstPx = pf.first * cmPx;
    auto rowStart = [&](size_t row) { return leftPx + (row == 0 ? firstPx : 0.0f) + listIndent; };
    auto availFor = [&](size_t row) { return std::max(20.0f, contentW - rowStart(row) - rightPx); };
    // Naechster Tabstopp rechts von p (Pixel ab dem linken Seitenrand); hinter
    // den eigenen Tabstopps gelten die Standardstopps alle 1,25 cm.
    auto nextTab = [&](float p) {
        for (float t : pf.tabs) {
            if (t * cmPx > p + 0.5f) return t * cmPx;
        }
        const float step = 1.25f * cmPx;
        return (std::floor(p / step + 0.001f) + 1.0f) * step;
    };
    struct Span {
        size_t b, e;
    };
    std::vector<Span> spans;
    {
        size_t rs = 0;
        float x = 0.0f;
        size_t lastBreak = kNone;
        auto tabWidth = [&](float at) {
            const float abs = rowStart(spans.size()) + at;
            return std::max(1.0f, nextTab(abs) - abs);
        };
        for (size_t k = 0; k < u.size(); ++k) {
            if (u[k].tab) u[k].adv = tabWidth(x);
            const float w = u[k].adv;
            if (u[k].kind != 1 && x + w > availFor(spans.size()) && k > rs) {
                const size_t cut = (lastBreak != kNone && lastBreak > rs) ? lastBreak : k;
                spans.push_back({rs, cut});
                rs = cut;
                x = 0.0f;
                for (size_t j = cut; j < k; ++j) {
                    if (u[j].tab) u[j].adv = tabWidth(x);
                    x += u[j].adv;
                }
                lastBreak = kNone;
            }
            x += w;
            if (u[k].kind == 1) lastBreak = k + 1;
        }
        spans.push_back({rs, u.size()});
    }

    const size_t lineEndPos = u.empty() ? L.contentBegin - L.begin : u.back().pos + u.back().len;
    for (size_t si = 0; si < spans.size(); ++si) {
        const Span sp = spans[si];
        const bool lastSpan = si + 1 == spans.size();
        float asc = 0.0f, desc = 0.0f;
        metrics(lineFont, heading ? linePx : basePx, &asc, &desc);
        for (size_t k = sp.b; k < sp.e; ++k) {
            asc = std::max(asc, u[k].ascent + std::max(0.0f, u[k].rise));
            desc = std::max(desc, u[k].descent + std::max(0.0f, -u[k].rise));
        }
        LinePart part;
        part.h = (asc + desc) * opt.lineSpacing;
        part.baseline = asc + (part.h - asc - desc) * 0.25f;
        part.firstStop = static_cast<uint32_t>(out.stops.size());
        part.firstFrag = static_cast<uint32_t>(out.frags.size());
        const int partIndex = static_cast<int>(out.parts.size());

        // Breite ohne haengende Leerzeichen, fuer die Ausrichtung
        size_t lastInk = sp.e;
        while (lastInk > sp.b && u[lastInk - 1].kind == 1) --lastInk;
        float rowW = 0.0f;
        size_t spaces = 0;
        for (size_t k = sp.b; k < lastInk; ++k) {
            rowW += u[k].adv;
            if (u[k].kind == 1) ++spaces;
        }
        const float avail = availFor(si);
        float offset = 0.0f, extra = 0.0f;
        switch (m.align) {
            case LineAlign::Center: offset = std::max(0.0f, (avail - rowW) * 0.5f); break;
            case LineAlign::Right: offset = std::max(0.0f, avail - rowW); break;
            case LineAlign::Justify:
                if (!lastSpan && spaces > 0 && avail > rowW) extra = (avail - rowW) / static_cast<float>(spaces);
                break;
            default: break;
        }
        float x = rowStart(si) + offset;

        if (si == 0 && list) {
            Frag b;
            b.kind = FragKind::Bullet;
            b.font = baseFont;
            b.px = basePx;
            b.x = leftPx + firstPx + listIndent * 0.2f;
            b.label = addLabel(m.kind == LineKind::Bullet ? std::string("\xE2\x80\xA2") : std::to_string(L.number) + ".");
            out.frags.push_back(b);
        }

        for (size_t k = sp.b; k < sp.e; ++k) {
            const LayUnit& lu = u[k];
            const size_t boundary = k == 0 ? lu.pos : u[k - 1].pos + u[k - 1].len;
            Stop stop;
            stop.pos = boundary;
            stop.x = x;
            stop.row = partIndex;
            stop.cls = lu.cls;
            out.stops.push_back(stop);

            const float adv = lu.tab ? std::max(1.0f, nextTab(x) - x) : lu.adv;
            const float w = adv + (lu.kind == 1 && !lu.tab && k < lastInk ? extra : 0.0f);
            if (lu.kind == 2) {
                Frag f;
                f.kind = FragKind::Atom;
                f.begin = lu.pos;
                f.end = lu.pos + lu.len;
                f.x = x;
                f.w = w;
                f.font = lu.font;
                f.px = lu.px;
                f.rise = lu.rise;
                f.token = lu.token;
                f.label = lu.label;
                f.atomKind = lu.atomKind;
                f.styleIndex = lu.style;
                out.frags.push_back(f);
            } else {
                // Zusammenhaengende Zeichen gleichen Formats in ein Stueck;
                // Leerzeichen stehen allein (Blocksatz verschiebt sie).
                Frag* prev = out.frags.size() > part.firstFrag ? &out.frags.back() : nullptr;
                const bool join = prev && prev->kind == FragKind::Text && lu.kind == 0 && prev->end == lu.pos &&
                                  prev->token == lu.token && prev->font == lu.font && prev->px == lu.px &&
                                  prev->chip == lu.chip && prev->rise == lu.rise &&
                                  text[L.begin + prev->end - 1] != ' ' && text[L.begin + prev->end - 1] != '\t' &&
                                  prev->styleIndex == lu.style;
                if (join) {
                    prev->end = lu.pos + lu.len;
                    prev->w += w;
                } else {
                    Frag f;
                    f.kind = FragKind::Text;
                    f.begin = lu.pos;
                    f.end = lu.pos + lu.len;
                    f.x = x;
                    f.w = w;
                    f.font = lu.font;
                    f.px = lu.px;
                    f.rise = lu.rise;
                    f.token = lu.token;
                    f.chip = lu.chip;
                    f.styleIndex = lu.style;
                    out.frags.push_back(f);
                }
            }
            x += w;
        }
        if (lastSpan) {
            Stop stop;
            stop.pos = lineEndPos;
            stop.x = x;
            stop.row = partIndex;
            stop.cls = 0;
            out.stops.push_back(stop);
        }
        part.stopCount = static_cast<uint32_t>(out.stops.size()) - part.firstStop;
        part.fragCount = static_cast<uint32_t>(out.frags.size()) - part.firstFrag;
        out.parts.push_back(part);
    }
}

void DocumentView::layout(ManuscriptDoc& doc, const DocOptions& opt, float viewWidth) {
    const std::string& text = doc.text();
    const std::vector<ManuscriptToken>& toks = doc.tokens();
    const std::vector<ManuscriptLine>& lines = doc.lines();

    const float z = opt.zoom;
    ++layoutPass_;
    stops_.clear();
    frags_.clear();
    rows_.clear();
    labels_.clear();
    lineFirstStop_.assign(lines.size(), kNone);
    lineBegins_.clear();
    for (const ManuscriptLine& L : lines) lineBegins_.push_back(L.begin);

    pageGap_ = std::max(12.0f, 20.0f * std::min(z, 1.0f));
    float contentW = 0.0f, topPad = 0.0f;
    if (opt.pageView) {
        pageWidthPx_ = opt.pageWidthCm * kPxPerCm * z;
        pageHeightPx_ = opt.pageHeightCm * kPxPerCm * z;
        marginPx_ = opt.marginCm * kPxPerCm * z;
        marginLeftPx_ = opt.marginLeftCm * kPxPerCm * z;
        marginRightPx_ = opt.marginRightCm * kPxPerCm * z;
        pageLeft_ = std::max(pageGap_, (viewWidth - pageWidthPx_) * 0.5f);
        docWidth_ = std::max(viewWidth, pageWidthPx_ + 2.0f * pageGap_);
        contentW = std::max(40.0f, pageWidthPx_ - marginLeftPx_ - marginRightPx_);
    } else {
        // Endlosrolle: volle Breite, schmaler Rand - wie die Weblayout-Ansicht
        pageLeft_ = 0.0f;
        pageWidthPx_ = viewWidth;
        marginPx_ = std::min(opt.marginCm * kPxPerCm * z, viewWidth * 0.08f);
        marginLeftPx_ = std::min(opt.marginLeftCm * kPxPerCm * z, viewWidth * 0.08f);
        marginRightPx_ = std::min(opt.marginRightCm * kPxPerCm * z, viewWidth * 0.08f);
        docWidth_ = viewWidth;
        contentW = std::max(80.0f, viewWidth - marginLeftPx_ - marginRightPx_);
        topPad = 20.0f * z;
    }
    const float pageContentH = pageHeightPx_ - 2.0f * marginPx_;
    const float contentLeft = pageLeft_ + marginLeftPx_;
    const float basePx = opt.fontPt * kPxPerPt * z;
    ImFont* baseFont = theme::familyFont(opt.font, false, false);
    ImFont* uiFont = theme::fonts().regular ? theme::fonts().regular : ImGui::GetFont();

    // Alles, was das Setzen einer Zeile ausser ihrem Text beeinflusst
    uint64_t setting = doc.projectSignature();
    setting = mixHash(setting, std::hash<std::string>{}(opt.font));
    for (float v : {opt.zoom, opt.fontPt, opt.lineSpacing, contentW})
        setting = mixHash(setting, static_cast<uint64_t>(v * 1000.0f));
    setting = mixHash(setting, (opt.readMode ? 1u : 0u) | (opt.showMarks ? 2u : 0u));
    setting = mixHash(setting, theme::fontGeneration());
    // Im Lesemodus haengen Werte vom Zeitpunkt ab - dort wird jedes Mal neu gesetzt.
    if (opt.readMode) setting = mixHash(setting, layoutPass_);

    int page = 0;
    float yIn = 0.0f;
    auto pageTop = [&](int p) { return pageGap_ + static_cast<float>(p) * (pageHeightPx_ + pageGap_); };
    auto placeRow = [&](Row& r) {
        if (opt.pageView) {
            if (yIn > 0.01f && yIn + r.h > pageContentH) {
                ++page;
                yIn = 0.0f;
            }
            r.page = page;
            r.y = pageTop(page) + marginPx_ + yIn;
        } else {
            r.page = 0;
            r.y = topPad + yIn;
        }
        yIn += r.h;
    };

    for (size_t li = 0; li < lines.size(); ++li) {
        const ManuscriptLine& L = lines[li];

        // ------------------------------------------------ Zeit- und Trennzeile
        if (L.kind == LineKind::Time || L.kind == LineKind::Break) {
            const bool isTime = L.kind == LineKind::Time;
            if (isTime && opt.readMode && !opt.showMarks) continue;
            Row r;
            r.line = static_cast<int>(li);
            ImFont* font = isTime ? uiFont : baseFont;
            const float px = basePx * (isTime ? 0.78f : 1.0f);
            ImFontBaked* baked = font->GetFontBaked(px);
            const float asc = baked->Ascent, desc = -baked->Descent;
            r.h = (asc + desc) * (isTime ? 1.7f : 2.0f);
            placeRow(r);
            r.baseline = r.y + (r.h - asc - desc) * 0.5f + asc;
            r.left = contentLeft;
            r.right = contentLeft + contentW;
            r.firstStop = stops_.size();
            r.firstFrag = frags_.size();

            std::string label = "* * *";
            if (isTime) {
                long long when = 0;
                for (size_t k = firstTokenAfter(toks, L.begin); k < toks.size(); ++k) {
                    if (toks[k].begin > L.end) break;
                    if (toks[k].kind == ManuscriptToken::Kind::Time) {
                        when = toks[k].time;
                        break;
                    }
                }
                label = formatStoryTime(when);
            }
            labels_.push_back(label);
            Frag f;
            f.kind = isTime ? FragKind::Label : FragKind::Rule;
            f.font = font;
            f.px = px;
            f.begin = L.begin;
            f.end = L.end;
            f.text = &labels_.back();
            f.style = &plainStyle();
            f.w = font->CalcTextSizeA(px, FLT_MAX, 0.0f, label.c_str()).x;
            f.x = contentLeft + (contentW - f.w) * 0.5f;
            frags_.push_back(f);

            lineFirstStop_[li] = stops_.size();
            Stop stop;
            stop.pos = L.begin;
            stop.x = contentLeft;
            stop.row = static_cast<int>(rows_.size());
            stop.cls = 3;
            stops_.push_back(stop);
            r.stopCount = 1;
            r.fragCount = 1;
            rows_.push_back(r);
            continue;
        }

        // ------------------------------------------------------ Textzeile
        const size_t base = firstTokenAfter(toks, L.contentBegin);
        uint64_t key = mixHash(setting, std::hash<std::string_view>{}(std::string_view(text).substr(L.begin, L.end - L.begin)));
        key = mixHash(key, static_cast<uint64_t>(L.kind) | static_cast<uint64_t>(L.level) << 8 |
                               static_cast<uint64_t>(L.align) << 16 | static_cast<uint64_t>(L.number) << 24);
        // Fett/kursiv koennen aus der Zeile davor hereinreichen
        if (base < toks.size() && toks[base].begin < L.contentBegin) key = mixHash(key, hashStyle(toks[base].style));

        auto found = lineCache_.find(key);
        if (found == lineCache_.end()) {
            found = lineCache_.emplace(key, LineLayout()).first;
            buildLine(doc, opt, li, contentW, found->second);
        }
        LineLayout& ll = found->second;
        ll.lastUsed = layoutPass_;

        if (ll.spaceBefore > 0.0f && yIn > 0.01f) yIn += ll.spaceBefore;
        for (const LinePart& part : ll.parts) {
            Row r;
            r.line = static_cast<int>(li);
            r.h = part.h;
            placeRow(r);
            r.baseline = r.y + part.baseline;
            r.left = contentLeft;
            r.right = contentLeft + contentW;
            r.firstStop = stops_.size();
            r.firstFrag = frags_.size();
            r.layout = &ll;
            r.lineBegin = L.begin;
            const int rowIndex = static_cast<int>(rows_.size());
            if (lineFirstStop_[li] == kNone && part.stopCount > 0) lineFirstStop_[li] = stops_.size();
            for (uint32_t k = part.firstStop; k < part.firstStop + part.stopCount; ++k) {
                Stop s = ll.stops[k];
                s.pos += L.begin;
                s.x += contentLeft;
                s.row = rowIndex;
                stops_.push_back(s);
            }
            for (uint32_t k = part.firstFrag; k < part.firstFrag + part.fragCount; ++k) {
                Frag f = ll.frags[k];
                f.begin += L.begin;
                f.end += L.begin;
                f.x += contentLeft;
                if (f.token >= 0) f.token += static_cast<int>(base);
                f.style = f.styleIndex >= 0 ? &ll.styles[static_cast<size_t>(f.styleIndex)] : &plainStyle();
                f.text = f.label >= 0 ? &ll.labels[static_cast<size_t>(f.label)] : nullptr;
                frags_.push_back(f);
            }
            r.stopCount = stops_.size() - r.firstStop;
            r.fragCount = frags_.size() - r.firstFrag;
            rows_.push_back(r);
        }
        yIn += ll.spaceAfter;
    }

    // Zeilen, die nicht mehr vorkommen, vergessen
    if (lineCache_.size() > lines.size() * 2 + 256) {
        for (auto it = lineCache_.begin(); it != lineCache_.end();) {
            if (it->second.lastUsed != layoutPass_)
                it = lineCache_.erase(it);
            else
                ++it;
        }
    }

    pageCount_ = page + 1;
    docHeight_ = opt.pageView ? pageTop(pageCount_) : yIn + 2.0f * topPad + 40.0f;
    layoutGeneration_ = doc.generation();
    layoutWidth_ = viewWidth;
    layoutTime_ = ImGui::GetTime();
}

size_t DocumentView::stopIndex(size_t pos) const {
    if (stops_.empty()) return 0;
    // Im versteckten Vorspann einer Zeile ("## ") steht der Cursor am Inhaltsanfang.
    if (!lineBegins_.empty()) {
        const auto it = std::upper_bound(lineBegins_.begin(), lineBegins_.end(), pos);
        const size_t li = static_cast<size_t>(it - lineBegins_.begin()) - (it == lineBegins_.begin() ? 0 : 1);
        if (li < lineFirstStop_.size() && lineFirstStop_[li] != kNone &&
            pos <= stops_[lineFirstStop_[li]].pos)
            return lineFirstStop_[li];
    }
    size_t lo = 0, hi = stops_.size();
    while (lo < hi) {
        const size_t mid = (lo + hi) / 2;
        if (stops_[mid].pos <= pos)
            lo = mid + 1;
        else
            hi = mid;
    }
    return lo == 0 ? 0 : lo - 1;
}

float DocumentView::stopX(size_t pos, int* row) const {
    if (stops_.empty()) {
        if (row) *row = 0;
        return 0.0f;
    }
    const Stop& s = stops_[stopIndex(pos)];
    if (row) *row = s.row;
    return s.x;
}

size_t DocumentView::hitTest(ImVec2 p) const {
    if (rows_.empty() || stops_.empty()) return 0;
    // Zeile unter dem Mauszeiger (oder die naechste)
    size_t best = 0;
    float bestDist = FLT_MAX;
    size_t lo = 0, hi = rows_.size();
    while (lo < hi) {
        const size_t mid = (lo + hi) / 2;
        if (rows_[mid].y + rows_[mid].h <= p.y)
            lo = mid + 1;
        else
            hi = mid;
    }
    for (size_t r = lo > 2 ? lo - 2 : 0; r < std::min(rows_.size(), lo + 3); ++r) {
        if (rows_[r].stopCount == 0) continue;
        const float top = rows_[r].y, bottom = rows_[r].y + rows_[r].h;
        const float d = p.y < top ? top - p.y : p.y > bottom ? p.y - bottom : 0.0f;
        if (d < bestDist) {
            bestDist = d;
            best = r;
        }
    }
    const Row& row = rows_[best];
    size_t pick = row.firstStop;
    float pickDist = FLT_MAX;
    for (size_t i = row.firstStop; i < row.firstStop + row.stopCount; ++i) {
        const float d = std::fabs(stops_[i].x - p.x);
        if (d < pickDist) {
            pickDist = d;
            pick = i;
        }
    }
    return stops_[pick].pos;
}

void DocumentView::ensureCaretVisible(float viewHeight) {
    if (stops_.empty() || rows_.empty()) return;
    int r = 0;
    const float x = stopX(cursor, &r);
    const Row& row = rows_[static_cast<size_t>(r)];
    const float sy = ImGui::GetScrollY();
    const float pad = row.h;
    if (row.y - pad < sy)
        ImGui::SetScrollY(std::max(0.0f, row.y - pad));
    else if (row.y + row.h + pad > sy + viewHeight)
        ImGui::SetScrollY(row.y + row.h + pad - viewHeight);
    const float sx = ImGui::GetScrollX();
    if (x < sx + 10.0f)
        ImGui::SetScrollX(std::max(0.0f, x - 40.0f));
    else if (x > sx + viewWidth_ - 10.0f)
        ImGui::SetScrollX(x - viewWidth_ + 40.0f);
}

// ------------------------------------------------------------------ Tasten
void DocumentView::handleKeys(ManuscriptDoc& doc, const DocOptions& opt, bool claimKeys) {
    ImGuiIO& io = ImGui::GetIO();
    // AltGr kommt unter Windows als Strg+Alt an - "@" und "EUR" duerfen keine
    // Tastenkuerzel ausloesen.
    const bool ctrl = io.KeyCtrl && !io.KeyAlt;
    const bool shift = io.KeyShift;
    auto press = [](ImGuiKey k) { return ImGui::IsKeyPressed(k, true); };
    auto relayout = [&]() {
        if (layoutGeneration_ != doc.generation()) layout(doc, opt, viewWidth_);
    };
    const size_t cursorBefore = cursor, anchorBefore = anchor;
    const uint64_t genBefore = doc.generation();

    auto horizontal = [&](int dir) {
        relayout();
        if (stops_.empty()) return;
        preferredX_ = -1.0f;
        if (hasSelection() && !shift) {
            moveCaret(dir < 0 ? selBegin() : selEnd(), false);
            return;
        }
        size_t i = stopIndex(cursor);
        const size_t n = stops_.size();
        if (dir < 0) {
            if (i == 0) {
                moveCaret(stops_[0].pos, shift);
                return;
            }
            if (ctrl) {
                size_t j = i;
                while (j > 0 && stops_[j - 1].cls == 1) --j;
                if (j > 0) {
                    const uint8_t c = stops_[j - 1].cls;
                    if (c == 0)
                        --j;
                    else
                        while (j > 0 && stops_[j - 1].cls == c) --j;
                }
                i = j;
            } else {
                --i;
            }
        } else {
            if (i + 1 >= n) {
                moveCaret(stops_[n - 1].pos, shift);
                return;
            }
            if (ctrl) {
                size_t j = i;
                const uint8_t c = stops_[j].cls;
                if (c == 0) {
                    ++j;
                } else {
                    while (j + 1 < n && stops_[j].cls == c) ++j;
                    while (j + 1 < n && stops_[j].cls == 1) ++j;
                }
                i = j;
            } else {
                ++i;
            }
        }
        moveCaret(stops_[i].pos, shift);
    };

    auto vertical = [&](int dir, float distance) {
        relayout();
        if (stops_.empty()) return;
        const size_t i = stopIndex(cursor);
        const Stop& s = stops_[i];
        const float x = preferredX_ >= 0.0f ? preferredX_ : s.x;
        size_t target = kNone;
        if (distance > 0.0f) {
            const Row& row = rows_[static_cast<size_t>(s.row)];
            const float y = row.y + row.h * 0.5f + static_cast<float>(dir) * distance;
            target = hitTest(ImVec2(x, y));
        } else {
            int t = s.row + dir;
            while (t >= 0 && t < static_cast<int>(rows_.size()) &&
                   rows_[static_cast<size_t>(t)].stopCount == 0)
                t += dir;
            if (t < 0) {
                target = stops_.front().pos;
            } else if (t >= static_cast<int>(rows_.size())) {
                target = stops_.back().pos;
            } else {
                const Row& row = rows_[static_cast<size_t>(t)];
                target = hitTest(ImVec2(x, row.y + row.h * 0.5f));
            }
        }
        moveCaret(target, shift);
        preferredX_ = x;
    };

    if (press(ImGuiKey_LeftArrow)) horizontal(-1);
    if (press(ImGuiKey_RightArrow)) horizontal(1);
    // Hoch/Runter gehoeren der Vorschlagsliste, solange sie offen ist.
    if (!claimKeys) {
        if (press(ImGuiKey_UpArrow)) vertical(-1, 0.0f);
        if (press(ImGuiKey_DownArrow)) vertical(1, 0.0f);
    }
    if (press(ImGuiKey_PageUp)) vertical(-1, viewHeight_ * 0.9f);
    if (press(ImGuiKey_PageDown)) vertical(1, viewHeight_ * 0.9f);
    if (press(ImGuiKey_Home)) {
        relayout();
        if (!stops_.empty()) {
            if (ctrl) {
                moveCaret(stops_.front().pos, shift);
            } else {
                const Row& row = rows_[static_cast<size_t>(stops_[stopIndex(cursor)].row)];
                moveCaret(stops_[row.firstStop].pos, shift);
            }
            preferredX_ = -1.0f;
        }
    }
    if (press(ImGuiKey_End)) {
        relayout();
        if (!stops_.empty()) {
            if (ctrl) {
                moveCaret(stops_.back().pos, shift);
            } else {
                const Row& row = rows_[static_cast<size_t>(stops_[stopIndex(cursor)].row)];
                moveCaret(stops_[row.firstStop + row.stopCount - 1].pos, shift);
            }
            preferredX_ = -1.0f;
        }
    }

    if (!claimKeys && (press(ImGuiKey_Enter) || press(ImGuiKey_KeypadEnter))) {
        if (opt.autoCorrect) autocorrect::beforeParagraph(doc, *this, opt.language);
        docops::newParagraph(doc, *this);
    }
    if (press(ImGuiKey_Backspace)) docops::backspace(doc, *this, ctrl);
    if (press(ImGuiKey_Delete)) docops::deleteForward(doc, *this, ctrl);
    if (!claimKeys && press(ImGuiKey_Tab)) docops::typeText(doc, *this, "\t", true);

    if (ctrl) {
        if (ImGui::IsKeyPressed(ImGuiKey_A, false)) {
            relayout();
            if (!stops_.empty()) select(stops_.front().pos, stops_.back().pos);
        }
        if (ImGui::IsKeyPressed(ImGuiKey_C, false) || ImGui::IsKeyPressed(ImGuiKey_Insert, false))
            docops::copy(doc, *this);
        if (ImGui::IsKeyPressed(ImGuiKey_X, false)) docops::cut(doc, *this);
        if (ImGui::IsKeyPressed(ImGuiKey_V, false)) docops::paste(doc, *this);
        if (press(ImGuiKey_Z)) {
            if (shift ? !doc.redo(this) : !doc.undo(this)) {
                if (shift)
                    doc.editor().redo();
                else
                    doc.editor().undo();
            }
        }
        if (press(ImGuiKey_Y)) {
            if (!doc.redo(this)) doc.editor().redo();
        }
        auto toggle = [&](bool TextStyle::*flag) {
            const bool on = docops::selectionHas(doc, *this, [&](const TextStyle& s) { return s.*flag; });
            docops::applyStyle(doc, *this, [&](TextStyle& s) {
                s.*flag = !on;
                if (flag == &TextStyle::superscript && !on) s.subscript = false;
                if (flag == &TextStyle::subscript && !on) s.superscript = false;
            });
        };
        if (ImGui::IsKeyPressed(ImGuiKey_B, false)) toggle(&TextStyle::bold);
        if (ImGui::IsKeyPressed(ImGuiKey_I, false)) toggle(&TextStyle::italic);
        if (ImGui::IsKeyPressed(ImGuiKey_U, false)) toggle(&TextStyle::underline);
        // Tasten nach ihrer Lage wie im deutschen Word: RightBracket ist die
        // "+"-Taste, Backslash die "#"-Taste, Oem102 die "<"-Taste.
        if (ImGui::IsKeyPressed(ImGuiKey_RightBracket, false)) toggle(&TextStyle::superscript);
        if (ImGui::IsKeyPressed(ImGuiKey_Backslash, false)) toggle(&TextStyle::subscript);
        if (ImGui::IsKeyPressed(ImGuiKey_Space, false))
            docops::applyStyle(doc, *this, [](TextStyle& s) { s = TextStyle(); });
        auto align = [&](LineAlign a) {
            const ManuscriptLine* line = docops::caretLine(doc, *this);
            docops::setAlign(doc, *this, line && line->align == a ? LineAlign::Left : a);
        };
        if (ImGui::IsKeyPressed(ImGuiKey_E, false)) align(LineAlign::Center);
        if (ImGui::IsKeyPressed(ImGuiKey_R, false)) align(LineAlign::Right);
        if (ImGui::IsKeyPressed(ImGuiKey_J, false)) align(LineAlign::Justify);
        if (ImGui::IsKeyPressed(ImGuiKey_L, false)) docops::setAlign(doc, *this, LineAlign::Left);
        auto grow = [&](float delta) {
            const float current = docops::caretStyle(doc, *this).size;
            const float base = current > 0.0f ? current : opt.fontPt;
            docops::applyStyle(doc, *this, [&](TextStyle& s) {
                const float from = s.size > 0.0f ? s.size : base;
                s.size = std::clamp(from + delta, 6.0f, 96.0f);
                if (s.size == opt.fontPt) s.size = 0.0f;
            });
        };
        // Strg+Umschalt+> / Strg+< (auf US-Tastaturen Strg+Umschalt+. bzw. ,)
        if (ImGui::IsKeyPressed(ImGuiKey_Oem102, true)) grow(shift ? 1.0f : -1.0f);
        if (shift && ImGui::IsKeyPressed(ImGuiKey_Period, true)) grow(1.0f);
        if (shift && ImGui::IsKeyPressed(ImGuiKey_Comma, true)) grow(-1.0f);
    }
    if (shift && !ctrl && ImGui::IsKeyPressed(ImGuiKey_Insert, false)) docops::paste(doc, *this);

    // Getippte Zeichen - auch mit AltGr (Strg+Alt), aber nicht mit Strg allein.
    if (!(io.KeyCtrl && !io.KeyAlt)) {
        std::string typed;
        for (ImWchar c : io.InputQueueCharacters) {
            if (c < 32 || c == 127) continue;
            char buf[8] = {0};
            ImTextCharToUtf8(buf, static_cast<unsigned int>(c));
            typed += buf;
        }
        if (!typed.empty()) {
            if (opt.autoCorrect) typed = autocorrect::transformTyped(doc.text(), cursor, typed, opt.language);
            docops::typeText(doc, *this, typed, true);
            if (opt.autoCorrect) autocorrect::afterTyped(doc, *this, typed, opt.language);
        }
    }

    if (cursor != cursorBefore || anchor != anchorBefore || doc.generation() != genBefore) {
        scrollToCaret_ = true;
        blinkStart_ = ImGui::GetTime();
    }
}

// ----------------------------------------------------------------- Zeichnen
DocEvent DocumentView::draw(ManuscriptDoc& doc, const DocOptions& opt, ImVec2 size,
                            const std::vector<DocHighlight>& highlights, bool claimKeys) {
    DocEvent ev;
    clickedThisFrame = false;
    zoomRequest = 0.0f;
    ImGuiIO& io = ImGui::GetIO();
    ImGuiContext& g = *GImGui;
    const ColorScheme& c = theme::colors();

    ImGui::PushStyleColor(ImGuiCol_ChildBg, c.workspaceColor);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGuiWindowFlags flags = ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoNav;
    if (io.KeyCtrl) flags |= ImGuiWindowFlags_NoScrollWithMouse;
    ImGui::BeginChild(id_.c_str(), size, ImGuiChildFlags_None, flags);
    ImGuiWindow* win = ImGui::GetCurrentWindow();
    if (wantFocus_) {
        ImGui::FocusWindow(win);
        wantFocus_ = false;
    }
    viewMin_ = win->InnerRect.Min;
    viewMax_ = win->InnerRect.Max;
    viewWidth_ = win->InnerRect.GetWidth();
    viewHeight_ = win->InnerRect.GetHeight();

    // ---------------------------------------------------------- Layout
    uint64_t sig = doc.projectSignature();
    sig = mixHash(sig, std::hash<std::string>{}(opt.font));
    for (float v : {opt.zoom, opt.pageWidthCm, opt.pageHeightCm, opt.marginCm, opt.marginLeftCm, opt.marginRightCm, opt.fontPt,
                    opt.lineSpacing})
        sig = mixHash(sig, static_cast<uint64_t>(v * 1000.0f));
    sig = mixHash(sig, (opt.readMode ? 1u : 0u) | (opt.showMarks ? 2u : 0u) | (opt.pageView ? 4u : 0u));
    sig = mixHash(sig, theme::fontGeneration());
    const bool widthChanged = std::fabs(layoutWidth_ - viewWidth_) > 0.5f;
    // Im Lesemodus haengen die Werte von der Zeit ab - darum ab und zu neu setzen.
    const bool stale = opt.readMode && ImGui::GetTime() - layoutTime_ > 0.5;
    if (layoutGeneration_ != doc.generation() || sig != layoutSig_ || widthChanged || stale) {
        layout(doc, opt, viewWidth_);
        layoutSig_ = sig;
    }

    float scrollX = ImGui::GetScrollX();
    float scrollY = ImGui::GetScrollY();
    scrollX_ = scrollX;

    // ------------------------------------------------------- Maus
    ImGui::SetCursorPos(ImVec2(scrollX, scrollY));
    ImGui::InvisibleButton("##doc", ImVec2(std::max(1.0f, viewWidth_), std::max(1.0f, viewHeight_)),
                           ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
    const ImGuiID itemId = ImGui::GetItemID();
    const bool hovered = ImGui::IsItemHovered();
    ImGui::SetCursorPos(ImVec2(0, 0));
    ImGui::Dummy(ImVec2(docWidth_, docHeight_));

    origin_ = ImVec2(win->Pos.x - scrollX, win->Pos.y - scrollY);
    const ImVec2 mouseDoc(io.MousePos.x - origin_.x, io.MousePos.y - origin_.y);

    const bool rootFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    const bool popupOpen =
        ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);
    // Ein anderes Feld war eben noch aktiv (Suchfeld, Schriftgrad): dessen
    // Enter darf nicht zusaetzlich hier einen Absatz machen.
    const bool otherItemBusy = (g.ActiveId != 0 && g.ActiveId != itemId) ||
                               (g.ActiveIdPreviousFrame != 0 && g.ActiveIdPreviousFrame != itemId);
    focused_ = activeView && rootFocused && !popupOpen && !opt.readMode && !otherItemBusy;

    if (hovered) {
        ImGui::SetMouseCursor(opt.readMode ? ImGuiMouseCursor_Arrow : ImGuiMouseCursor_TextInput);
        if (io.KeyCtrl && io.MouseWheel != 0.0f) zoomRequest = io.MouseWheel * 0.1f;
    }

    bool overClickable = false;  // wird beim Zeichnen gesetzt
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        clickedThisFrame = true;
        activeView = true;
        if (!opt.readMode && !(io.KeyCtrl && !io.KeyAlt)) {
            const size_t pos = hitTest(mouseDoc);
            const int clicks = io.MouseClickedCount[ImGuiMouseButton_Left];
            if (clicks >= 3 && !stops_.empty()) {
                // ganzer Absatz
                const int row = stops_[stopIndex(pos)].row;
                const int line = rows_[static_cast<size_t>(row)].line;
                size_t b = stopIndex(pos), e = b;
                while (b > 0 && rows_[static_cast<size_t>(stops_[b - 1].row)].line == line) --b;
                while (e + 1 < stops_.size() &&
                       rows_[static_cast<size_t>(stops_[e + 1].row)].line == line)
                    ++e;
                select(stops_[b].pos, stops_[e].pos);
            } else if (clicks == 2 && !stops_.empty()) {
                size_t k = stopIndex(pos);
                uint8_t cls = stops_[k].cls;
                if (cls == 0 && k > 0) {
                    --k;
                    cls = stops_[k].cls;
                }
                size_t b = k, e = k;
                while (b > 0 && stops_[b - 1].cls == cls) --b;
                while (e + 1 < stops_.size() && stops_[e].cls == cls) ++e;
                select(stops_[b].pos, stops_[e].pos);
            } else {
                moveCaret(pos, io.KeyShift);
                dragging_ = true;
            }
            preferredX_ = -1.0f;
        }
    }
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        clickedThisFrame = true;
        activeView = true;
        if (!opt.readMode) {
            const size_t pos = hitTest(mouseDoc);
            if (!hasSelection() || pos < selBegin() || pos > selEnd()) moveCaret(pos, false);
            ev.kind = DocEvent::Kind::ContextMenu;
            ev.pos = pos;
            if (opt.spellCheck && !stops_.empty()) {
                const Row& row = rows_[static_cast<size_t>(stops_[stopIndex(pos)].row)];
                if (row.layout && row.layout->spellGeneration == spell::generation()) {
                    for (const auto& issue : row.layout->spellIssues) {
                        const size_t a = row.lineBegin + issue.first, b = row.lineBegin + issue.second;
                        if (a <= pos && pos <= b) {
                            ev.spellBegin = a;
                            ev.spellEnd = b;
                            ev.spellWord = doc.text().substr(a, b - a);
                            break;
                        }
                    }
                }
            }
        }
    }
    if (dragging_) {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            cursor = hitTest(mouseDoc);
            // am Rand weiterrollen
            const float top = win->InnerRect.Min.y, bottom = win->InnerRect.Max.y;
            if (io.MousePos.y < top) ImGui::SetScrollY(scrollY - (top - io.MousePos.y) * 0.5f);
            if (io.MousePos.y > bottom) ImGui::SetScrollY(scrollY + (io.MousePos.y - bottom) * 0.5f);
        } else {
            dragging_ = false;
        }
    }

    // ------------------------------------------------------ Tastatur
    if (focused_) handleKeys(doc, opt, claimKeys);
    if (layoutGeneration_ != doc.generation()) layout(doc, opt, viewWidth_);
    cursor = std::min(cursor, doc.text().size());
    anchor = std::min(anchor, doc.text().size());

    if (scrollToCaret_) {
        ensureCaretVisible(viewHeight_);
        scrollToCaret_ = false;
    }

    // ------------------------------------------------------- Zeichnen
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 o = origin_;
    const std::string& text = doc.text();
    const std::vector<ManuscriptToken>& toks = doc.tokens();
    const Project& project = doc.project();
    const ImU32 pageCol = theme::u32(c.pageColor);
    const ImU32 inkCol = theme::u32(c.pageTextColor);
    const ImU32 shadowCol = theme::u32(theme::withAlpha(c.textPrimary, theme::isLightTheme() ? 0.14f : 0.45f));
    const float visTop = scrollY, visBottom = scrollY + viewHeight_;

    if (opt.pageView) {
        for (int p = 0; p < pageCount_; ++p) {
            const float top = pageGap_ + static_cast<float>(p) * (pageHeightPx_ + pageGap_);
            if (top > visBottom || top + pageHeightPx_ < visTop) continue;
            const ImVec2 a(o.x + pageLeft_, o.y + top);
            const ImVec2 b(a.x + pageWidthPx_, a.y + pageHeightPx_);
            dl->AddRectFilled(ImVec2(a.x + 2, a.y + 3), ImVec2(b.x + 3, b.y + 4), shadowCol, 1.0f);
            dl->AddRectFilled(a, b, pageCol);
            // dezente Seitenzahl im unteren Rand
            char num[16];
            std::snprintf(num, sizeof(num), "%d", p + 1);
            ImFont* f = theme::fonts().regular ? theme::fonts().regular : ImGui::GetFont();
            const float px = std::max(9.0f, 10.0f * kPxPerPt * opt.zoom);
            const float w = f->CalcTextSizeA(px, FLT_MAX, 0.0f, num).x;
            dl->AddText(f, px, ImVec2((a.x + b.x - w) * 0.5f, b.y - marginPx_ * 0.55f),
                        theme::u32(theme::withAlpha(c.pageTextColor, 0.45f)), num);
        }
    } else {
        dl->AddRectFilled(ImVec2(o.x, o.y), ImVec2(o.x + docWidth_, o.y + std::max(docHeight_, viewHeight_ + scrollY)),
                          pageCol);
    }

    // sichtbare Zeilen
    size_t firstRow = 0;
    {
        size_t lo = 0, hi = rows_.size();
        while (lo < hi) {
            const size_t mid = (lo + hi) / 2;
            if (rows_[mid].y + rows_[mid].h < visTop)
                lo = mid + 1;
            else
                hi = mid;
        }
        firstRow = lo;
    }
    size_t lastRow = firstRow;
    while (lastRow < rows_.size() && rows_[lastRow].y <= visBottom) ++lastRow;

    auto rowEndX = [&](const Row& row) {
        if (row.stopCount == 0) return row.left;
        return stops_[row.firstStop + row.stopCount - 1].x;
    };
    // Bereich im Quelltext -> Rechtecke pro Zeile
    auto rangeRects = [&](size_t b, size_t e, ImU32 col) {
        if (b >= e || stops_.empty()) return;
        int rb = 0, re = 0;
        const float xb = stopX(b, &rb);
        const float xe = stopX(e, &re);
        const int from = std::max(rb, static_cast<int>(firstRow));
        const int to = std::min(re, static_cast<int>(lastRow) - 1);
        for (int r = from; r <= to; ++r) {
            const Row& row = rows_[static_cast<size_t>(r)];
            if (row.stopCount == 0) continue;
            float x0 = r == rb ? xb : stops_[row.firstStop].x;
            float x1 = r == re ? xe : rowEndX(row) + (r < re ? row.h * 0.25f : 0.0f);
            if (x1 <= x0) x1 = x0 + row.h * 0.25f;
            dl->AddRectFilled(ImVec2(o.x + x0, o.y + row.y), ImVec2(o.x + x1, o.y + row.y + row.h), col);
        }
    };

    // Hintergrundfarben aus dem Text (Hervorhebung)
    for (size_t r = firstRow; r < lastRow; ++r) {
        const Row& row = rows_[r];
        for (size_t k = row.firstFrag; k < row.firstFrag + row.fragCount; ++k) {
            const Frag& f = frags_[k];
            if (f.kind != FragKind::Text || f.style->background.empty()) continue;
            dl->AddRectFilled(ImVec2(o.x + f.x, o.y + row.y + row.h * 0.08f),
                              ImVec2(o.x + f.x + f.w, o.y + row.y + row.h * 0.96f),
                              hexColor(f.style->background, 0));
        }
    }
    for (const DocHighlight& h : highlights) {
        if (h.end < h.begin) continue;
        rangeRects(h.begin, h.end, h.color);
    }
    if (hasSelection() && !opt.readMode) {
        const ImVec4 sel = theme::withAlpha(c.accentColor, focused_ ? 0.28f : 0.16f);
        rangeRects(selBegin(), selEnd(), theme::u32(sel));
    }

    // Text
    ImFont* uiFont = theme::fonts().regular ? theme::fonts().regular : ImGui::GetFont();
    const bool clicked = hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
    const bool ctrlClick = clicked && io.KeyCtrl && !io.KeyAlt;
    for (size_t r = firstRow; r < lastRow; ++r) {
        const Row& row = rows_[r];
        for (size_t k = row.firstFrag; k < row.firstFrag + row.fragCount; ++k) {
            const Frag& f = frags_[k];
            const ImU32 col = hexColor(f.style->color, inkCol);
            ImFontBaked* baked = f.font->GetFontBaked(f.px);
            const float top = row.baseline - baked->Ascent - f.rise;
            const ImVec2 p(o.x + f.x, o.y + top);
            const ImVec2 rectMin(o.x + f.x, o.y + row.y);
            const ImVec2 rectMax(o.x + f.x + std::max(f.w, 4.0f), o.y + row.y + row.h);
            const bool over = hovered && ImGui::IsMouseHoveringRect(rectMin, rectMax, false);
            const ManuscriptToken* tok = f.token >= 0 && static_cast<size_t>(f.token) < toks.size()
                                             ? &toks[static_cast<size_t>(f.token)]
                                             : nullptr;
            switch (f.kind) {
                case FragKind::Text: {
                    if (f.chip && tok) {
                        // Element-/Wertmarke im Schreibmodus: getoente Flaeche + Unterstrich
                        const ImVec4 chip = tok->resolved ? project.elementColor(tok->targetId) : c.errorColor;
                        dl->AddRectFilled(ImVec2(rectMin.x - 1, rectMin.y + row.h * 0.1f),
                                          ImVec2(rectMax.x + 1, rectMax.y - row.h * 0.06f),
                                          theme::u32(theme::withAlpha(chip, over ? 0.28f : 0.15f)), 3.0f);
                        dl->AddLine(ImVec2(rectMin.x - 1, o.y + row.baseline + f.px * 0.18f),
                                    ImVec2(rectMax.x + 1, o.y + row.baseline + f.px * 0.18f),
                                    theme::u32(chip),
                                    tok->kind == ManuscriptToken::Kind::Value ? 2.0f : 1.2f);
                        if (over) {
                            overClickable = true;
                            if (!tok->resolved)
                                ImGui::SetTooltip("%s", TR("Nicht gefunden - Name pruefen."));
                            else if (tok->kind == ManuscriptToken::Kind::Value)
                                ImGui::SetTooltip("%s.%s = %s  (%s)\n%s",
                                                  project.displayName(tok->targetId).c_str(),
                                                  tok->field.c_str(),
                                                  project.valueAt(tok->targetId, tok->field, tok->time).c_str(),
                                                  formatStoryTime(tok->time).c_str(),
                                                  TR("Strg+Klick oeffnet die Details."));
                            else
                                ImGui::SetTooltip("%s\n%s", project.elementPath(tok->targetId).c_str(),
                                                  TR("Strg+Klick oeffnet die Details."));
                            if (ctrlClick && tok->resolved) {
                                ev.kind = DocEvent::Kind::Element;
                                ev.id = tok->targetId;
                            }
                        }
                    }
                    if (f.end > f.begin && text[f.begin] != '\t')
                        dl->AddText(f.font, f.px, p, col, text.c_str() + f.begin, text.c_str() + f.end);
                    const float thick = std::max(1.0f, f.px / 15.0f);
                    if (f.style->underline) {
                        const float y = o.y + row.baseline - f.rise + f.px * 0.12f;
                        dl->AddLine(ImVec2(p.x, y), ImVec2(p.x + f.w, y), col, thick);
                    }
                    if (f.style->strike) {
                        const float y = o.y + row.baseline - f.rise - baked->Ascent * 0.3f;
                        dl->AddLine(ImVec2(p.x, y), ImVec2(p.x + f.w, y), col, thick);
                    }
                    break;
                }
                case FragKind::Atom: {
                    const float h = row.h * 0.72f;
                    const float cy = o.y + row.baseline - baked->Ascent * 0.35f;
                    if (f.atomKind == 3) {
                        // Lesemodus: eingesetzter Wert, anklickbar
                        const std::string& label = *f.text;
                        if (over) {
                            dl->AddRectFilled(rectMin, rectMax, theme::u32(theme::withAlpha(c.accentColor, 0.12f)), 3.0f);
                            overClickable = true;
                            if (tok && tok->resolved) {
                                ImGui::SetTooltip("%s", project.elementPath(tok->targetId).c_str());
                                if (clicked) {
                                    ev.kind = DocEvent::Kind::Element;
                                    ev.id = tok->targetId;
                                }
                            }
                        }
                        dl->AddText(f.font, f.px, p, col, label.c_str());
                        if (f.style->underline) {
                            const float y = o.y + row.baseline + f.px * 0.12f;
                            dl->AddLine(ImVec2(p.x, y), ImVec2(p.x + f.w, y), col, std::max(1.0f, f.px / 15.0f));
                        }
                    } else if (f.atomKind == 0) {
                        // Aktion: kleines Schild
                        const std::string& label = *f.text;
                        const ImVec4 tint = tok && tok->resolved ? c.warningColor : c.errorColor;
                        const ImVec2 a(o.x + f.x + f.px * 0.15f, cy - h * 0.5f);
                        const ImVec2 b(o.x + f.x + f.w - f.px * 0.15f, cy + h * 0.5f);
                        dl->AddRectFilled(a, b, theme::u32(theme::withAlpha(tint, over ? 0.35f : 0.2f)), h * 0.3f);
                        const float tw = uiFont->CalcTextSizeA(f.px, FLT_MAX, 0.0f, label.c_str()).x;
                        const ImFontBaked* ub = uiFont->GetFontBaked(f.px);
                        dl->AddText(uiFont, f.px, ImVec2((a.x + b.x - tw) * 0.5f, cy - (ub->Ascent - ub->Descent) * 0.5f),
                                    theme::u32(c.textPrimary), label.c_str());
                        if (over) {
                            overClickable = true;
                            const Action* act = tok ? project.action(tok->targetId) : nullptr;
                            if (act)
                                ImGui::SetTooltip("%s\n%s\n%s", act->title.c_str(),
                                                  formatStoryTime(project.resolveActionTime(*act)).c_str(),
                                                  TR("Strg+Klick zeigt die Aktion."));
                            else
                                ImGui::SetTooltip("%s", TR("Aktion nicht gefunden."));
                            if ((ctrlClick || (clicked && opt.readMode)) && act) {
                                ev.kind = DocEvent::Kind::Action;
                                ev.id = act->id;
                            }
                        }
                    } else if (f.atomKind == 1) {
                        // Lesezeichen: Faehnchen
                        const float s = f.px * 0.55f;
                        const float x0 = o.x + f.x + f.w * 0.3f;
                        const ImU32 flagCol = theme::u32(c.accentColor);
                        dl->AddLine(ImVec2(x0, cy - s), ImVec2(x0, cy + s * 0.8f), flagCol, 1.5f);
                        dl->AddTriangleFilled(ImVec2(x0, cy - s), ImVec2(x0 + s * 0.9f, cy - s * 0.6f),
                                              ImVec2(x0, cy - s * 0.2f), flagCol);
                        if (over && tok) {
                            overClickable = true;
                            ImGui::SetTooltip("%s: %s", TR("Lesezeichen"), tok->field.c_str());
                        }
                    } else {
                        // Kommentar: Sprechblase
                        const float s = f.px * 0.42f;
                        const float cx = o.x + f.x + f.w * 0.5f;
                        const ImU32 fill = theme::u32(theme::withAlpha(c.warningColor, over ? 0.95f : 0.75f));
                        dl->AddRectFilled(ImVec2(cx - s, cy - s * 0.8f), ImVec2(cx + s, cy + s * 0.55f), fill, s * 0.35f);
                        dl->AddTriangleFilled(ImVec2(cx - s * 0.5f, cy + s * 0.5f), ImVec2(cx - s * 0.1f, cy + s * 0.5f),
                                              ImVec2(cx - s * 0.6f, cy + s * 1.05f), fill);
                        if (over && tok) {
                            overClickable = true;
                            ImGui::SetTooltip("%s\n\n%s", tok->field.c_str(), TR("Klick bearbeitet den Kommentar."));
                            if (clicked) {
                                ev.kind = DocEvent::Kind::Note;
                                ev.pos = tok->begin;
                                ev.end = tok->end;
                                ev.text = tok->field;
                            }
                        }
                    }
                    break;
                }
                case FragKind::Label: {
                    // Zeitmarke: "--- Tag 5, 14:00 ---"
                    const std::string& label = *f.text;
                    const ImU32 lc = theme::u32(theme::withAlpha(c.textSecondary, 0.9f));
                    const float mid = o.y + row.y + row.h * 0.5f;
                    dl->AddLine(ImVec2(o.x + row.left, mid), ImVec2(o.x + f.x - f.px * 0.6f, mid),
                                theme::u32(theme::withAlpha(c.textSecondary, 0.35f)));
                    dl->AddLine(ImVec2(o.x + f.x + f.w + f.px * 0.6f, mid), ImVec2(o.x + row.right, mid),
                                theme::u32(theme::withAlpha(c.textSecondary, 0.35f)));
                    const ImFontBaked* lb = f.font->GetFontBaked(f.px);
                    dl->AddText(f.font, f.px, ImVec2(o.x + f.x, mid - (lb->Ascent - lb->Descent) * 0.5f), lc,
                                label.c_str());
                    if (hovered && ImGui::IsMouseHoveringRect(ImVec2(o.x + row.left, o.y + row.y),
                                                              ImVec2(o.x + row.right, o.y + row.y + row.h), false))
                        ImGui::SetTooltip("%s", TR("Ab hier gilt dieser Zeitpunkt der Geschichte."));
                    break;
                }
                case FragKind::Rule: {
                    const std::string& label = *f.text;
                    const ImFontBaked* rb = f.font->GetFontBaked(f.px);
                    const float mid = o.y + row.y + row.h * 0.5f;
                    dl->AddText(f.font, f.px, ImVec2(o.x + f.x, mid - (rb->Ascent - rb->Descent) * 0.5f),
                                theme::u32(theme::withAlpha(c.pageTextColor, 0.7f)), label.c_str());
                    break;
                }
                case FragKind::Bullet: {
                    const std::string& label = *f.text;
                    ImFontBaked* bb = f.font->GetFontBaked(f.px);
                    dl->AddText(f.font, f.px, ImVec2(o.x + f.x, o.y + row.baseline - bb->Ascent), inkCol,
                                label.c_str());
                    break;
                }
            }
        }
    }
    if (overClickable && io.KeyCtrl) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

    // ------------------------------------------------- Rechtschreibung
    // Geprueft wird nur, was sichtbar ist, und hoechstens ein paar
    // Millisekunden pro Frame - der Rest folgt in den naechsten Frames.
    if (opt.spellCheck && !opt.readMode && spell::available()) {
        const uint64_t generation = spell::generation();
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(4);
        for (size_t r = firstRow; r < lastRow; ++r) {
            LineLayout* ll = rows_[r].layout;
            if (!ll || ll->spellGeneration == generation) continue;
            if (std::chrono::steady_clock::now() > deadline) break;
            ll->spellIssues.clear();
            for (const spell::Issue& issue : spell::check(ll->plain)) {
                if (issue.end == 0 || issue.end > ll->plainPos.size()) continue;
                ll->spellIssues.push_back({ll->plainPos[issue.begin], ll->plainPos[issue.end - 1] + 1});
            }
            ll->spellGeneration = generation;
        }
        const ImU32 wave = theme::u32(c.errorColor);
        for (size_t r = firstRow; r < lastRow; ++r) {
            const Row& row = rows_[r];
            if (!row.layout || row.layout->spellGeneration != generation || row.stopCount == 0) continue;
            const Stop& firstStop = stops_[row.firstStop];
            const Stop& lastStop = stops_[row.firstStop + row.stopCount - 1];
            for (const auto& issue : row.layout->spellIssues) {
                const size_t a = row.lineBegin + issue.first, b = row.lineBegin + issue.second;
                if (b <= firstStop.pos || a > lastStop.pos) continue;
                // Das Wort, an dem gerade getippt wird, bleibt unmarkiert.
                if (focused_ && !hasSelection() && cursor >= a && cursor <= b) continue;
                const float x0 = a <= firstStop.pos ? firstStop.x : stopX(a, nullptr);
                const float x1 = b >= lastStop.pos ? lastStop.x : stopX(b, nullptr);
                if (x1 <= x0 + 1.0f) continue;
                const float y = o.y + row.baseline + std::max(2.0f, opt.fontPt * kPxPerPt * opt.zoom * 0.16f);
                const float step = std::max(2.0f, 2.2f * opt.zoom);
                ImVec2 pts[256];
                int count = 0;
                for (float x = o.x + x0; x <= o.x + x1 && count < 256; x += step) {
                    pts[count] = ImVec2(x, y + ((count % 2) ? step * 0.6f : 0.0f));
                    ++count;
                }
                if (count >= 2) dl->AddPolyline(pts, count, wave, 0, std::max(1.0f, opt.zoom));
            }
        }
    }

    // ------------------------------------------------------- Cursor
    caretValid_ = false;
    if (!stops_.empty() && !opt.readMode) {
        int r = 0;
        const float x = stopX(cursor, &r);
        const Row& row = rows_[static_cast<size_t>(r)];
        caretScreen_ = ImVec2(o.x + x, o.y + row.y);
        caretHeight_ = row.h;
        caretPage_ = row.page;
        caretValid_ = true;
        const bool blinkOn = !io.ConfigInputTextCursorBlink ||
                             std::fmod(ImGui::GetTime() - blinkStart_, 1.1) < 0.65;
        if (focused_ && blinkOn && !hasSelection()) {
            const float inset = row.h * 0.1f;
            dl->AddLine(ImVec2(caretScreen_.x, caretScreen_.y + inset),
                        ImVec2(caretScreen_.x, caretScreen_.y + row.h - inset * 0.5f), inkCol,
                        std::max(1.0f, opt.zoom * 1.2f));
        }
        if (focused_) {
            // Eingabemethoden (Umlaute per Tottaste, IME) an die richtige Stelle
            ImGuiPlatformImeData& ime = g.PlatformImeData;
            ime.WantVisible = true;
            ime.WantTextInput = true;
            ime.InputPos = ImVec2(caretScreen_.x - 1.0f, caretScreen_.y);
            ime.InputLineHeight = row.h;
            ime.ViewportId = win->Viewport->ID;
        }
    }

    rulerOrigin_ = ImVec2(o.x + pageLeft_, o.y);
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
    return ev;
}

// ------------------------------------------------------------------ Lineal
// Wie in Word: graue Raender, weisser Textbereich, darauf die Einzugsmarken
// des Absatzes am Cursor (oben Erstzeile, unten haengend/links mit Kaestchen,
// rechts) und seine Tabstopps. Ziehen aendert, Klick setzt einen Tabstopp,
// einen Tabstopp aus dem Lineal ziehen entfernt ihn.
void DocumentView::drawRuler(ManuscriptDoc& doc, const DocOptions& opt, float height) {
    const ColorScheme& c = theme::colors();
    ImGuiIO& io = ImGui::GetIO();
    const ImVec2 p0 = ImGui::GetCursorScreenPos();
    const float width = std::max(1.0f, ImGui::GetContentRegionAvail().x);
    ImGui::InvisibleButton("##ruler", ImVec2(width, height));
    const bool hovered = ImGui::IsItemHovered();
    const bool active = ImGui::IsItemActive();
    const bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 p1(p0.x + width, p0.y + height);
    dl->PushClipRect(p0, p1, true);
    dl->AddRectFilled(p0, p1, theme::u32(c.workspaceColor));

    const float z = opt.zoom;
    const float cm = kPxPerCm * z;
    const float pageX = rulerOrigin_.x;
    const float pageW = opt.pageView ? opt.pageWidthCm * cm : viewWidth_;
    const float textL = pageX + marginLeftPx_;
    const float textR = pageX + pageW - marginRightPx_;
    const float top = p0.y + 2.0f, bottom = p1.y - 2.0f;
    const float mid = (top + bottom) * 0.5f;
    dl->AddRectFilled(ImVec2(pageX, top + 3.0f), ImVec2(pageX + pageW, bottom - 3.0f),
                      theme::u32(theme::mix(c.pageColor, c.workspaceColor, 0.45f)));
    dl->AddRectFilled(ImVec2(textL, top + 3.0f), ImVec2(textR, bottom - 3.0f), theme::u32(c.pageColor));

    // Teilstriche ab dem linken Rand, wie in Word
    const ImU32 tick = theme::u32(theme::withAlpha(c.pageTextColor, 0.55f));
    ImFont* font = theme::fonts().regular ? theme::fonts().regular : ImGui::GetFont();
    const float fontPx = std::min(height * 0.45f, 11.0f);
    const int quarters = static_cast<int>(std::ceil(pageW / (cm * 0.25f))) + 1;
    for (int side = -1; side <= 1; side += 2) {
        for (int q = side < 0 ? 1 : 0; q <= quarters; ++q) {
            const float x = textL + static_cast<float>(side * q) * cm * 0.25f;
            if (x < pageX || x > pageX + pageW) continue;
            if (q % 4 == 0) {
                if (q == 0) continue;
                char num[8];
                std::snprintf(num, sizeof(num), "%d", q / 4);
                const ImVec2 ts = font->CalcTextSizeA(fontPx, FLT_MAX, 0.0f, num);
                dl->AddText(font, fontPx, ImVec2(x - ts.x * 0.5f, mid - ts.y * 0.5f), tick, num);
            } else {
                const float len = (q % 2 == 0 ? 0.22f : 0.1f) * (bottom - top);
                dl->AddLine(ImVec2(x, mid - len), ImVec2(x, mid + len), tick);
            }
        }
    }

    // Absatz am Cursor
    const ManuscriptLine* line = opt.readMode ? nullptr : docops::caretLine(doc, *this);
    const bool editable = line && line->kind != LineKind::Time && line->kind != LineKind::Break;
    const ParagraphFormat pf = editable ? line->format : ParagraphFormat();
    auto snap = [&](float v) { return io.KeyAlt ? std::round(v * 100.0f) / 100.0f : std::round(v * 4.0f) / 4.0f; };
    const float mouseCm = (io.MousePos.x - textL) / cm;

    const float firstX = textL + (pf.left + pf.first) * cm;
    const float leftX = textL + pf.left * cm;
    const float rightX = textR - pf.right * cm;
    const float s = std::max(4.0f, height * 0.22f);  // Groesse der Dreiecke

    // --------------------------------------------------- Greifen
    auto near = [&](float x, float y0, float y1) {
        return std::fabs(io.MousePos.x - x) <= s + 1.0f && io.MousePos.y >= y0 && io.MousePos.y <= y1;
    };
    auto hitMarker = [&]() -> RulerDrag {
        if (editable) {
            if (near(firstX, top, mid - 1.0f)) return RulerDrag::First;
            if (near(leftX, bottom - s * 0.9f, bottom + 2.0f)) return RulerDrag::LeftBoth;
            if (near(leftX, mid, bottom - s * 0.9f)) return RulerDrag::Hanging;
            if (near(rightX, mid, bottom + 2.0f)) return RulerDrag::Right;
            for (float t : pf.tabs) {
                if (std::fabs(io.MousePos.x - (textL + t * cm)) <= 4.0f && io.MousePos.y >= mid) return RulerDrag::Tab;
            }
        }
        if (opt.pageView && std::fabs(io.MousePos.x - textL) <= 4.0f) return RulerDrag::MarginLeft;
        if (opt.pageView && std::fabs(io.MousePos.x - textR) <= 4.0f) return RulerDrag::MarginRight;
        return RulerDrag::None;
    };
    const RulerDrag under = hovered && rulerDrag_ == RulerDrag::None ? hitMarker() : RulerDrag::None;
    if (under == RulerDrag::MarginLeft || under == RulerDrag::MarginRight || rulerDrag_ == RulerDrag::MarginLeft ||
        rulerDrag_ == RulerDrag::MarginRight)
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    else if (under != RulerDrag::None || rulerDrag_ != RulerDrag::None)
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

    if (clicked) {
        rulerDrag_ = hitMarker();
        if (rulerDrag_ == RulerDrag::Tab) {
            float best = FLT_MAX;
            for (float t : pf.tabs) {
                const float d = std::fabs(io.MousePos.x - (textL + t * cm));
                if (d < best) {
                    best = d;
                    rulerTabFrom_ = t;
                }
            }
        } else if (rulerDrag_ == RulerDrag::None && editable && io.MousePos.x > textL && io.MousePos.x < textR) {
            // Klick ins Lineal: neuer Tabstopp an dieser Stelle - direkt zum Weiterziehen
            const float at = snap(mouseCm);
            if (at > 0.0f) {
                docops::setParagraphFormat(doc, *this, [&](ParagraphFormat& f) {
                    bool exists = false;
                    for (float t : f.tabs) exists = exists || std::fabs(t - at) < 0.01f;
                    if (!exists) f.tabs.push_back(at);
                });
                rulerDrag_ = RulerDrag::Tab;
                rulerTabFrom_ = at;
            }
        }
    }

    // --------------------------------------------------- Ziehen
    float value = 0.0f;  // neuer Wert in cm fuer die Anzeige
    bool removeTab = false;
    if (rulerDrag_ != RulerDrag::None) {
        const float x = io.MousePos.x;
        switch (rulerDrag_) {
            case RulerDrag::MarginLeft: value = std::clamp(snap((x - pageX) / cm), 0.0f, pageW / cm - opt.marginRightCm - 2.0f); break;
            case RulerDrag::MarginRight: value = std::clamp(snap((pageX + pageW - x) / cm), 0.0f, pageW / cm - opt.marginLeftCm - 2.0f); break;
            case RulerDrag::Right: value = snap((textR - x) / cm); break;
            case RulerDrag::Tab:
                value = snap(mouseCm);
                removeTab = io.MousePos.y > p1.y + height || io.MousePos.y < p0.y - height || value <= 0.0f;
                break;
            default: value = snap(mouseCm); break;  // First, Hanging, LeftBoth: Position ab dem linken Rand
        }
        const float guideX = [&]() {
            switch (rulerDrag_) {
                case RulerDrag::MarginLeft: return pageX + value * cm;
                case RulerDrag::MarginRight: return pageX + pageW - value * cm;
                case RulerDrag::Right: return textR - value * cm;
                default: return textL + value * cm;
            }
        }();
        // gestrichelte Hilfslinie ueber die Seite
        if (!removeTab) {
            ImDrawList* fg = ImGui::GetForegroundDrawList();
            for (float y = viewMin_.y; y < viewMax_.y; y += 8.0f)
                fg->AddLine(ImVec2(guideX, y), ImVec2(guideX, std::min(y + 4.0f, viewMax_.y)),
                            theme::u32(theme::withAlpha(c.pageTextColor, 0.6f)));
        }
        const char* what = "";
        switch (rulerDrag_) {
            case RulerDrag::MarginLeft: what = TR("Seitenrand links"); break;
            case RulerDrag::MarginRight: what = TR("Seitenrand rechts"); break;
            case RulerDrag::First: what = TR("Erste Zeile"); break;
            case RulerDrag::Hanging: what = TR("Haengender Einzug"); break;
            case RulerDrag::LeftBoth: what = TR("Einzug links"); break;
            case RulerDrag::Right: what = TR("Einzug rechts"); break;
            case RulerDrag::Tab: what = removeTab ? TR("Tabstopp entfernen") : TR("Tabstopp"); break;
            default: break;
        }
        if (removeTab)
            ImGui::SetTooltip("%s", what);
        else
            ImGui::SetTooltip("%s: %.2f cm", what, value);

        if (!active && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            // Loslassen: uebernehmen - ein Undo-Schritt
            AppSettings& settings = theme::settings();
            switch (rulerDrag_) {
                case RulerDrag::MarginLeft:
                    settings.docMarginLeftCm = value;
                    theme::save();
                    break;
                case RulerDrag::MarginRight:
                    settings.docMarginRightCm = value;
                    theme::save();
                    break;
                case RulerDrag::First:
                    docops::setParagraphFormat(doc, *this, [&](ParagraphFormat& f) { f.first = value - f.left; });
                    break;
                case RulerDrag::Hanging:
                    // Linker Einzug wandert, die erste Zeile bleibt stehen
                    docops::setParagraphFormat(doc, *this, [&](ParagraphFormat& f) {
                        const float firstLine = f.left + f.first;
                        f.left = value;
                        f.first = firstLine - value;
                    });
                    break;
                case RulerDrag::LeftBoth:
                    docops::setParagraphFormat(doc, *this, [&](ParagraphFormat& f) { f.left = value; });
                    break;
                case RulerDrag::Right:
                    docops::setParagraphFormat(doc, *this, [&](ParagraphFormat& f) { f.right = value; });
                    break;
                case RulerDrag::Tab: {
                    const float from = rulerTabFrom_;
                    docops::setParagraphFormat(doc, *this, [&](ParagraphFormat& f) {
                        f.tabs.erase(std::remove_if(f.tabs.begin(), f.tabs.end(),
                                                    [&](float t) { return std::fabs(t - from) < 0.01f; }),
                                     f.tabs.end());
                        if (!removeTab) f.tabs.push_back(value);
                    });
                    break;
                }
                default: break;
            }
            rulerDrag_ = RulerDrag::None;
            requestFocus();
        }
    }

    // --------------------------------------------------- Marken zeichnen
    const ImU32 marker = theme::u32(c.textSecondary);
    const ImU32 markerFill = theme::u32(theme::mix(c.pageColor, c.textSecondary, 0.25f));
    auto downTriangle = [&](float x, bool ghost) {
        const ImVec2 a(x - s, top), b(x + s, top), d(x, top + s * 1.1f);
        dl->AddTriangleFilled(a, b, d, ghost ? theme::u32(theme::withAlpha(c.accentColor, 0.5f)) : markerFill);
        dl->AddTriangle(a, b, d, marker);
    };
    auto upTriangle = [&](float x, float base, bool ghost) {
        const ImVec2 a(x - s, base), b(x + s, base), d(x, base - s * 1.1f);
        dl->AddTriangleFilled(a, b, d, ghost ? theme::u32(theme::withAlpha(c.accentColor, 0.5f)) : markerFill);
        dl->AddTriangle(a, b, d, marker);
    };
    auto tabMark = [&](float x, bool ghost) {
        const ImU32 col = ghost ? theme::u32(c.accentColor) : theme::u32(c.pageTextColor);
        dl->AddLine(ImVec2(x, mid + 1.0f), ImVec2(x, bottom - 1.0f), col, 2.0f);
        dl->AddLine(ImVec2(x, bottom - 1.0f), ImVec2(x + s * 0.9f, bottom - 1.0f), col, 2.0f);
    };

    if (editable) {
        // Standard-Tabstopps hinter dem letzten eigenen: kleine graue Striche
        const float lastTab = pf.tabs.empty() ? 0.0f : pf.tabs.back();
        for (float t = (std::floor(lastTab / 1.25f) + 1.0f) * 1.25f; textL + t * cm < textR; t += 1.25f)
            dl->AddLine(ImVec2(textL + t * cm, bottom - 3.0f), ImVec2(textL + t * cm, bottom), marker);

        for (float t : pf.tabs) {
            const bool dragged = rulerDrag_ == RulerDrag::Tab && std::fabs(t - rulerTabFrom_) < 0.01f;
            if (!dragged) tabMark(textL + t * cm, false);
        }
        const float baseLeft = bottom - s * 0.9f;
        downTriangle(firstX, false);
        upTriangle(leftX, baseLeft, false);
        dl->AddRectFilled(ImVec2(leftX - s, baseLeft), ImVec2(leftX + s, bottom + 1.0f), markerFill);
        dl->AddRect(ImVec2(leftX - s, baseLeft), ImVec2(leftX + s, bottom + 1.0f), marker);
        upTriangle(rightX, bottom, false);

        // Vorschau der gezogenen Marke
        switch (rulerDrag_) {
            case RulerDrag::First: downTriangle(textL + value * cm, true); break;
            case RulerDrag::Hanging:
            case RulerDrag::LeftBoth: upTriangle(textL + value * cm, baseLeft, true); break;
            case RulerDrag::Right: upTriangle(textR - value * cm, bottom, true); break;
            case RulerDrag::Tab:
                if (!removeTab) tabMark(textL + value * cm, true);
                break;
            default: break;
        }
    }
    dl->PopClipRect();

    if (hovered && rulerDrag_ == RulerDrag::None) {
        const char* tip = nullptr;
        switch (under) {
            case RulerDrag::First: tip = TR("Einzug der ersten Zeile"); break;
            case RulerDrag::Hanging: tip = TR("Haengender Einzug - die erste Zeile bleibt stehen"); break;
            case RulerDrag::LeftBoth: tip = TR("Einzug links - verschiebt den ganzen Absatz"); break;
            case RulerDrag::Right: tip = TR("Einzug rechts"); break;
            case RulerDrag::Tab: tip = TR("Tabstopp - ziehen verschiebt, aus dem Lineal ziehen entfernt"); break;
            case RulerDrag::MarginLeft:
            case RulerDrag::MarginRight: tip = TR("Seitenrand - ziehen, um ihn zu verschieben"); break;
            default:
                if (editable && io.MousePos.x > textL && io.MousePos.x < textR)
                    tip = TR("Klicken setzt einen Tabstopp (Alt: ohne Einrasten)");
                break;
        }
        if (tip) ImGui::SetTooltip("%s", tip);
    }
}

}  // namespace se
