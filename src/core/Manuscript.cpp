#include "core/Manuscript.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <string_view>

#include "core/StoryTime.h"

namespace se {

bool TextStyle::operator==(const TextStyle& o) const {
    return bold == o.bold && italic == o.italic && underline == o.underline && strike == o.strike &&
           superscript == o.superscript && subscript == o.subscript && color == o.color &&
           background == o.background && size == o.size && font == o.font;
}

namespace {

bool isNameChar(unsigned char c) {
    return std::isalnum(c) || c == '_' || c == '-' || c == '/' ||
           c >= 0x80;  // Umlaute und andere UTF-8-Folgebytes
}

bool atLineStart(const std::string& text, size_t pos) {
    if (pos == 0) return true;
    return text[pos - 1] == '\n';
}

char lower(char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); }

size_t lineEndOf(const std::string& text, size_t from) {
    const size_t e = text.find('\n', from);
    return e == std::string::npos ? text.size() : e;
}

// Sucht `needle` nur bis `limit` (z.B. Zeilenende) - ohne Treffer in der Zeile
// wuerde find() sonst den ganzen restlichen Text durchlaufen.
size_t findBefore(const std::string& text, const char* needle, size_t from, size_t limit) {
    const size_t p = std::string_view(text.data(), std::min(limit, text.size())).find(needle, from);
    return p == std::string_view::npos ? std::string::npos : p;
}

// "Alice" oder "Characters/Main/Alice" -> Element-ID
std::string resolveElement(const Project& p, const std::string& reference) {
    if (reference.empty()) return std::string();
    for (const Element& el : p.elements) {
        if (p.elementPath(el.id) == reference) return el.id;
    }
    for (const Element& el : p.elements) {
        if (el.name == reference) return el.id;
    }
    return std::string();
}

// Gibt es im selben Absatz ein passendes Schlusszeichen? Nur dann ist das
// Sternchen eine Hervorhebung und kein normales Satzzeichen. Hervorhebungen
// gelten nie ueber eine Leerzeile hinweg - so faerbt ein vergessenes
// Sternchen nicht den Rest des Buches ein. Gesucht wird nur bis dorthin (und
// nicht erst das Absatzende bestimmt): ein Text ohne Leerzeilen waere sonst
// fuer jedes Sternchen einmal komplett zu durchlaufen.
bool hasClosingMark(const std::string& text, size_t after, size_t markLength) {
    for (size_t i = after; i + markLength <= text.size(); ++i) {
        if (text[i] == '\n' && i + 1 < text.size() && text[i + 1] == '\n') return false;
        if (text[i] != '*') continue;
        const bool doubleMark = i + 1 < text.size() && text[i + 1] == '*';
        if (markLength == 2 && !doubleMark) continue;
        if (markLength == 1 && doubleMark) {
            ++i;  // "**" ist hier kein Ende fuer ein einfaches Sternchen
            continue;
        }
        // Ein Schlusszeichen klebt am Wort: " *" waere ein neuer Anfang.
        if (i > after && text[i - 1] != ' ' && text[i - 1] != '\n') return true;
    }
    return false;
}

// Eine Zeile, die nur aus Strichen besteht, trennt zwei Szenen.
bool isSceneBreakLine(const std::string& text, size_t lineBegin, size_t lineEnd) {
    size_t dashes = 0;
    for (size_t i = lineBegin; i < lineEnd; ++i) {
        const char c = text[i];
        if (c == '-')
            ++dashes;
        else if (c != ' ' && c != '\t' && c != '\r')
            return false;
    }
    return dashes >= 3;
}

bool isHeadingStart(const std::string& text, size_t i) {
    return text[i] == '#' && atLineStart(text, i) && i + 1 < text.size() &&
           (text[i + 1] == '#' || text[i + 1] == ' ');
}

LineAlign alignFromName(const std::string& name) {
    if (name == "center") return LineAlign::Center;
    if (name == "right") return LineAlign::Right;
    if (name == "justify") return LineAlign::Justify;
    return LineAlign::Left;
}

// "%%pf:align=center;left=1.5;right=0;first=-0.63;tabs=2.5,5%%" (ohne die %%)
void parseParagraphBody(const std::string& body, ParagraphFormat& f) {
    size_t i = 0;
    while (i < body.size()) {
        size_t semi = body.find(';', i);
        if (semi == std::string::npos) semi = body.size();
        const std::string item = body.substr(i, semi - i);
        const size_t eq = item.find('=');
        if (eq != std::string::npos) {
            const std::string key = item.substr(0, eq);
            const std::string value = item.substr(eq + 1);
            if (key == "align") {
                f.align = alignFromName(value);
            } else if (key == "left") {
                f.left = static_cast<float>(std::atof(value.c_str()));
            } else if (key == "right") {
                f.right = static_cast<float>(std::atof(value.c_str()));
            } else if (key == "first") {
                f.first = static_cast<float>(std::atof(value.c_str()));
            } else if (key == "tabs") {
                size_t t = 0;
                while (t < value.size()) {
                    size_t comma = value.find(',', t);
                    if (comma == std::string::npos) comma = value.size();
                    const float pos = static_cast<float>(std::atof(value.substr(t, comma - t).c_str()));
                    if (pos > 0.0f) f.tabs.push_back(pos);
                    t = comma + 1;
                }
                std::sort(f.tabs.begin(), f.tabs.end());
            }
        }
        i = semi + 1;
    }
}

// Steht am Zeilenende eine Absatzmarke (Ausrichtung, Einzuege, Tabstopps)?
// Liefert ihren Anfang und fuellt `format`.
bool paragraphSuffix(const std::string& text, size_t lineBegin, size_t lineEnd, size_t* markerBegin,
                     ParagraphFormat* format) {
    size_t end = lineEnd;
    if (end > lineBegin && text[end - 1] == '\r') --end;
    if (markerBegin) *markerBegin = lineEnd;
    if (end - lineBegin < 4 || text.compare(end - 2, 2, "%%") != 0) return false;
    // Anfang der letzten %%...%%-Marke der Zeile
    const size_t open = end - 2 > lineBegin ? text.rfind("%%", end - 3) : std::string::npos;
    if (open == std::string::npos || open < lineBegin || open + 4 > end) return false;
    const std::string body = text.substr(open + 2, end - 2 - open - 2);
    ParagraphFormat f;
    if (body.rfind("pf:", 0) == 0) {
        parseParagraphBody(body.substr(3), f);
    } else if (body == "center" || body == "right" || body == "justify" || body == "left") {
        f.align = alignFromName(body);
    } else {
        return false;
    }
    if (markerBegin) *markerBegin = open;
    if (format) *format = f;
    return true;
}

std::string formatCm(float v) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.2f", v);
    std::string s = buf;
    while (!s.empty() && s.back() == '0') s.pop_back();
    if (!s.empty() && s.back() == '.') s.pop_back();
    return s == "-0" ? "0" : s;
}

bool isHexColor(const std::string& s) {
    if (s.size() != 7 || s[0] != '#') return false;
    for (size_t i = 1; i < s.size(); ++i) {
        if (!std::isxdigit(static_cast<unsigned char>(s[i]))) return false;
    }
    return true;
}

std::string trimmedCopy(const std::string& s) {
    const size_t b = s.find_first_not_of(" \t\"'");
    if (b == std::string::npos) return std::string();
    const size_t e = s.find_last_not_of(" \t\"'");
    return s.substr(b, e - b + 1);
}

// "color:#C00000; font-size:14pt" -> Stil. Unbekanntes bleibt unbeachtet.
void applySpanStyle(const std::string& declarations, TextStyle& style) {
    size_t i = 0;
    while (i < declarations.size()) {
        size_t semi = declarations.find(';', i);
        if (semi == std::string::npos) semi = declarations.size();
        const std::string decl = declarations.substr(i, semi - i);
        const size_t colon = decl.find(':');
        if (colon != std::string::npos) {
            std::string key = trimmedCopy(decl.substr(0, colon));
            const std::string value = trimmedCopy(decl.substr(colon + 1));
            for (char& ch : key) ch = lower(ch);
            if (key == "color" && isHexColor(value)) {
                style.color = value;
            } else if ((key == "background" || key == "background-color") && isHexColor(value)) {
                style.background = value;
            } else if (key == "font-size") {
                const float v = static_cast<float>(std::atof(value.c_str()));
                if (v > 0.0f) style.size = v;
            } else if (key == "font-family" && !value.empty()) {
                style.font = value;
            }
        }
        i = semi + 1;
    }
}

std::string formatSize(float size) {
    char buf[32];
    if (size == static_cast<float>(static_cast<int>(size)))
        std::snprintf(buf, sizeof(buf), "%d", static_cast<int>(size));
    else
        std::snprintf(buf, sizeof(buf), "%.1f", size);
    return buf;
}

}  // namespace

std::vector<ManuscriptToken> parseManuscript(const Project& p, const std::string& text) {
    std::vector<ManuscriptToken> out;
    long long currentTime = 0;
    size_t textStart = 0;

    // Zeichenformat, das gerade gilt. Fett und kursiv reichen bis zum Ende des
    // Absatzes, alles andere endet spaetestens an der Zeile - der Serializer
    // schliesst ohnehin jede Zeile ab.
    bool bold = false;
    bool italic = false;
    bool strike = false;
    bool underline = false;
    bool superscript = false;
    bool subscript = false;
    std::vector<TextStyle> spans;  // verschachtelte <span style=...>

    auto currentStyle = [&]() {
        TextStyle s = spans.empty() ? TextStyle() : spans.back();
        s.bold = bold;
        s.italic = italic;
        s.strike = strike;
        s.underline = underline;
        s.superscript = superscript;
        s.subscript = subscript;
        return s;
    };
    auto lineStylesActive = [&]() {
        return strike || underline || superscript || subscript || !spans.empty();
    };
    auto resetAll = [&]() {
        bold = italic = strike = underline = superscript = subscript = false;
        spans.clear();
    };
    auto stamp = [&](ManuscriptToken& t) {
        t.style = currentStyle();
        t.bold = bold;
        t.italic = italic;
    };

    auto flushText = [&](size_t upTo) {
        if (upTo <= textStart) return;
        ManuscriptToken t;
        t.kind = ManuscriptToken::Kind::Text;
        t.begin = textStart;
        t.end = upTo;
        t.raw = text.substr(textStart, upTo - textStart);
        t.time = currentTime;
        stamp(t);
        out.push_back(t);
    };
    // Formatzeichen: unsichtbar, aber als eigene Marke, damit der Editor und
    // die Lesefassung wissen, wo sie stehen.
    auto markup = [&](size_t from, size_t to) {
        ManuscriptToken t;
        t.kind = ManuscriptToken::Kind::Markup;
        t.begin = from;
        t.end = to;
        t.raw = text.substr(from, to - from);
        t.time = currentTime;
        t.resolved = true;
        out.push_back(t);
    };

    size_t i = 0;
    while (i < text.size()) {
        const char c = text[i];

        // ---------------------------------------------------- Ueberschrift
        if (isHeadingStart(text, i)) {
            const size_t lineEnd = lineEndOf(text, i);
            size_t markerBegin = lineEnd;
            paragraphSuffix(text, i, lineEnd, &markerBegin, nullptr);
            flushText(i);
            ManuscriptToken t;
            t.kind = ManuscriptToken::Kind::Heading;
            t.begin = i;
            t.end = markerBegin;
            t.raw = text.substr(i, markerBegin - i);
            t.time = currentTime;
            t.resolved = true;
            out.push_back(t);
            if (markerBegin < lineEnd) markup(markerBegin, lineEnd);
            resetAll();  // eine Ueberschrift beendet jede Hervorhebung
            i = lineEnd;
            textStart = i;
            continue;
        }

        // ---------------------------------------------------- Szenenwechsel
        if (c == '-' && atLineStart(text, i)) {
            const size_t lineEnd = lineEndOf(text, i);
            if (isSceneBreakLine(text, i, lineEnd)) {
                flushText(i);
                ManuscriptToken t;
                t.kind = ManuscriptToken::Kind::Break;
                t.begin = i;
                t.end = lineEnd;
                t.raw = text.substr(i, lineEnd - i);
                t.time = currentTime;
                t.resolved = true;
                out.push_back(t);
                resetAll();
                i = lineEnd;
                textStart = i;
                continue;
            }
        }

        // ---------------------------------------------------- Zeitmarke
        if (c == '#' && i + 1 < text.size() && text[i + 1] != '#' && text[i + 1] != ' ') {
            const size_t lineEnd = lineEndOf(text, i);
            const std::string body = trim(text.substr(i + 1, lineEnd - i - 1));
            long long parsed = 0;
            if (parseStoryTime(body, &parsed)) {
                flushText(i);
                const size_t markerEnd = lineEnd < text.size() ? lineEnd + 1 : lineEnd;
                ManuscriptToken t;
                t.kind = ManuscriptToken::Kind::Time;
                t.begin = i;
                t.end = markerEnd;
                t.time = parsed;
                t.resolved = true;
                t.raw = text.substr(i, markerEnd - i);
                out.push_back(t);
                currentTime = parsed;
                i = markerEnd;
                textStart = i;
                continue;
            }
        }

        // ---------------------------------------------------- Aktion
        if (c == '!' && text.compare(i, 5, "!act:") == 0) {
            size_t j = i + 5;
            while (j < text.size() && isNameChar(static_cast<unsigned char>(text[j]))) ++j;
            flushText(i);
            ManuscriptToken t;
            t.kind = ManuscriptToken::Kind::Action;
            t.begin = i;
            t.end = j;
            t.raw = text.substr(i, j - i);
            t.targetId = text.substr(i + 5, j - i - 5);
            t.time = currentTime;
            t.resolved = p.action(t.targetId) != nullptr;
            stamp(t);
            out.push_back(t);
            i = j;
            textStart = i;
            continue;
        }

        // ------------------------------------ Lesezeichen, Kommentar, Marke
        if (c == '%' && i + 1 < text.size() && text[i + 1] == '%') {
            const size_t lineEnd = lineEndOf(text, i);
            const size_t close = findBefore(text, "%%", i + 2, lineEnd);
            if (close != std::string::npos) {
                flushText(i);
                const std::string body = text.substr(i + 2, close - i - 2);
                ManuscriptToken t;
                t.begin = i;
                t.end = close + 2;
                t.raw = text.substr(i, t.end - i);
                t.time = currentTime;
                t.resolved = true;
                if (body.rfind("bm:", 0) == 0) {
                    t.kind = ManuscriptToken::Kind::Bookmark;
                    t.field = body.substr(3);
                } else if (body.rfind("note:", 0) == 0) {
                    t.kind = ManuscriptToken::Kind::Note;
                    t.field = body.substr(5);
                } else {
                    t.kind = ManuscriptToken::Kind::Markup;
                }
                stamp(t);
                out.push_back(t);
                i = t.end;
                textStart = i;
                continue;
            }
        }

        // ---------------------------------------------------- Hervorhebung
        if (c == '*') {
            const size_t length = (i + 1 < text.size() && text[i + 1] == '*') ? 2 : 1;
            bool& flag = length == 2 ? bold : italic;
            const bool closing = flag;
            // Aufmachen nur, wenn direkt ein Wort folgt und der Absatz das
            // Zeichen auch wieder schliesst.
            const size_t after = i + length;
            const bool opening = !flag && after < text.size() && text[after] != ' ' &&
                                 text[after] != '\n' && hasClosingMark(text, after, length);
            if (closing || opening) {
                flushText(i);
                markup(i, after);
                flag = !flag;
                i = after;
                textStart = i;
                continue;
            }
        }

        // ---------------------------------------------------- Durchgestrichen
        if (c == '~' && i + 1 < text.size() && text[i + 1] == '~') {
            const size_t after = i + 2;
            const size_t lineEnd = lineEndOf(text, i);
            const size_t close = findBefore(text, "~~", after, lineEnd);
            const bool opening = !strike && after < lineEnd && close != std::string::npos;
            if (strike || opening) {
                flushText(i);
                markup(i, after);
                strike = !strike;
                i = after;
                textStart = i;
                continue;
            }
        }

        // ------------------------------------------------ HTML-Auszeichnung
        if (c == '<') {
            struct Tag {
                const char* text;
                bool* flag;
                bool value;
            };
            const Tag tags[] = {
                {"<u>", &underline, true},       {"</u>", &underline, false},
                {"<sup>", &superscript, true},   {"</sup>", &superscript, false},
                {"<sub>", &subscript, true},     {"</sub>", &subscript, false},
            };
            bool matched = false;
            for (const Tag& tag : tags) {
                const size_t len = std::char_traits<char>::length(tag.text);
                if (text.compare(i, len, tag.text) == 0) {
                    flushText(i);
                    markup(i, i + len);
                    *tag.flag = tag.value;
                    i += len;
                    textStart = i;
                    matched = true;
                    break;
                }
            }
            if (matched) continue;

            if (text.compare(i, 7, "</span>") == 0) {
                flushText(i);
                markup(i, i + 7);
                if (!spans.empty()) spans.pop_back();
                i += 7;
                textStart = i;
                continue;
            }
            static const char kSpanOpen[] = "<span style=\"";
            const size_t openLen = sizeof(kSpanOpen) - 1;
            if (text.compare(i, openLen, kSpanOpen) == 0) {
                const size_t lineEnd = lineEndOf(text, i);
                const size_t close = findBefore(text, "\">", i + openLen, lineEnd);
                if (close != std::string::npos) {
                    flushText(i);
                    markup(i, close + 2);
                    TextStyle next = spans.empty() ? TextStyle() : spans.back();
                    applySpanStyle(text.substr(i + openLen, close - i - openLen), next);
                    spans.push_back(next);
                    i = close + 2;
                    textStart = i;
                    continue;
                }
            }
        }

        // ---------------------------------------------------- Element / Wert
        if (c == '@') {
            size_t j = i + 1;
            while (j < text.size() && isNameChar(static_cast<unsigned char>(text[j]))) ++j;
            std::string reference = text.substr(i + 1, j - i - 1);
            std::string field;
            if (j < text.size() && text[j] == '.') {
                size_t k = j + 1;
                while (k < text.size() && isNameChar(static_cast<unsigned char>(text[k]))) ++k;
                if (k > j + 1) {
                    field = text.substr(j + 1, k - j - 1);
                    j = k;
                }
            }
            if (!reference.empty()) {
                flushText(i);
                ManuscriptToken t;
                t.kind =
                    field.empty() ? ManuscriptToken::Kind::Element : ManuscriptToken::Kind::Value;
                t.begin = i;
                t.end = j;
                t.raw = text.substr(i, j - i);
                t.targetId = resolveElement(p, reference);
                t.field = field;
                t.time = currentTime;
                t.resolved = !t.targetId.empty();
                stamp(t);
                out.push_back(t);
                i = j;
                textStart = i;
                continue;
            }
        }

        if (c == '\n') {
            // Unterstreichung, Farbe & Co. enden an der Zeile ...
            if (lineStylesActive()) {
                flushText(i);
                strike = underline = superscript = subscript = false;
                spans.clear();
                textStart = i;
            }
            // ... fett und kursiv an der Leerzeile - siehe paragraphEnd().
            if (i + 1 < text.size() && text[i + 1] == '\n' && (bold || italic)) {
                flushText(i);
                bold = italic = false;
                textStart = i;
            }
        }

        ++i;
    }
    flushText(text.size());
    return out;
}

// ------------------------------------------------------------------ Zeilen
std::vector<ManuscriptLine> manuscriptLines(const std::string& text) {
    std::vector<ManuscriptLine> lines;
    int number = 0;
    size_t b = 0;
    while (true) {
        const size_t e = lineEndOf(text, b);
        ManuscriptLine L;
        L.begin = b;
        L.end = e;
        L.contentBegin = b;
        L.contentEnd = e;
        if (e > b && text[e - 1] == '\r') L.contentEnd = e - 1;

        if (b < text.size() && isHeadingStart(text, b)) {
            L.kind = LineKind::Heading;
            size_t k = b;
            while (k < e && text[k] == '#') ++k;
            L.level = static_cast<int>(k - b);
            while (k < e && text[k] == ' ') ++k;
            L.contentBegin = k;
        } else if (e > b && text[b] == '-' && isSceneBreakLine(text, b, e)) {
            L.kind = LineKind::Break;
        } else if (e > b + 1 && text[b] == '#' && text[b + 1] != '#' && text[b + 1] != ' ') {
            long long parsed = 0;
            if (parseStoryTime(trim(text.substr(b + 1, e - b - 1)), &parsed)) L.kind = LineKind::Time;
        } else if (e >= b + 2 && text[b] == '-' && text[b + 1] == ' ') {
            L.kind = LineKind::Bullet;
            L.contentBegin = b + 2;
        } else {
            size_t k = b;
            while (k < e && std::isdigit(static_cast<unsigned char>(text[k]))) ++k;
            if (k > b && k + 1 < e && text[k] == '.' && text[k + 1] == ' ') {
                L.kind = LineKind::Numbered;
                L.contentBegin = k + 2;
            }
        }

        if (L.kind == LineKind::Body || L.kind == LineKind::Heading || L.kind == LineKind::Bullet ||
            L.kind == LineKind::Numbered) {
            size_t markerBegin = L.contentEnd;
            paragraphSuffix(text, L.contentBegin, e, &markerBegin, &L.format);
            L.align = L.format.align;
            if (markerBegin < L.contentEnd) L.contentEnd = markerBegin;
        }
        if (L.contentEnd < L.contentBegin) L.contentEnd = L.contentBegin;

        number = L.kind == LineKind::Numbered ? number + 1 : 0;
        L.number = number;
        lines.push_back(L);

        if (e >= text.size()) break;
        b = e + 1;
    }
    return lines;
}

size_t lineIndexAt(const std::vector<ManuscriptLine>& lines, size_t offset) {
    if (lines.empty()) return 0;
    size_t lo = 0, hi = lines.size();
    while (hi - lo > 1) {
        const size_t mid = (lo + hi) / 2;
        if (lines[mid].begin <= offset)
            lo = mid;
        else
            hi = mid;
    }
    return lo;
}

std::string linePrefix(LineKind kind, int level) {
    switch (kind) {
        case LineKind::Heading: return std::string(static_cast<size_t>(std::max(1, level)), '#') + " ";
        case LineKind::Bullet: return "- ";
        case LineKind::Numbered: return "1. ";
        default: return std::string();
    }
}

std::string paragraphMarker(const ParagraphFormat& f) {
    if (f.left == 0.0f && f.right == 0.0f && f.first == 0.0f && f.tabs.empty()) return alignMarker(f.align);
    std::string body = "pf:";
    static const char* names[] = {"left", "center", "right", "justify"};
    if (f.align != LineAlign::Left) body += std::string("align=") + names[static_cast<int>(f.align)] + ";";
    if (f.left != 0.0f) body += "left=" + formatCm(f.left) + ";";
    if (f.right != 0.0f) body += "right=" + formatCm(f.right) + ";";
    if (f.first != 0.0f) body += "first=" + formatCm(f.first) + ";";
    if (!f.tabs.empty()) {
        body += "tabs=";
        for (size_t i = 0; i < f.tabs.size(); ++i) body += (i ? "," : "") + formatCm(f.tabs[i]);
        body += ";";
    }
    body.pop_back();
    return "%%" + body + "%%";
}

std::string alignMarker(LineAlign align) {
    switch (align) {
        case LineAlign::Center: return "%%center%%";
        case LineAlign::Right: return "%%right%%";
        case LineAlign::Justify: return "%%justify%%";
        default: return std::string();
    }
}

// ---------------------------------------------------------------- Serializer
std::string serializeUnits(const std::vector<StyledUnit>& units, std::vector<size_t>* positions) {
    std::string out;
    if (positions) positions->assign(units.size(), 0);

    // Gruppen gleichen Formats. Leerzeichen am Rand einer Gruppe gehoeren vor
    // bzw. hinter die Sternchen: "** x**" wuerde der Parser nicht als
    // Hervorhebung lesen.
    struct Group {
        size_t begin, end, coreBegin, coreEnd;
    };
    auto isSpace = [&](size_t k) { return units[k].raw == " " || units[k].raw == "\t"; };
    std::vector<Group> groups;
    for (size_t i = 0; i < units.size();) {
        size_t j = i + 1;
        while (j < units.size() && units[j].style == units[i].style) ++j;
        Group g{i, j, i, j};
        while (g.coreBegin < j && isSpace(g.coreBegin)) ++g.coreBegin;
        while (g.coreEnd > g.coreBegin && isSpace(g.coreEnd - 1)) --g.coreEnd;
        groups.push_back(g);
        i = j;
    }

    // Fett und kursiv werden nur umgeschaltet, wenn sich etwas aendert - so
    // entstehen nie mehrdeutige Folgen wie "****", und hoechstens drei
    // Sternchen stehen nebeneinander (die der Parser als "**" + "*" liest).
    bool bold = false, italic = false;
    for (size_t gi = 0; gi < groups.size(); ++gi) {
        const Group& g = groups[gi];
        const TextStyle& s = units[g.begin].style;
        const bool hasCore = g.coreBegin < g.coreEnd;

        std::string span;
        if (!s.color.empty()) span += "color:" + s.color + ";";
        if (!s.background.empty()) span += "background:" + s.background + ";";
        if (s.size > 0.0f) span += "font-size:" + formatSize(s.size) + "pt;";
        if (!s.font.empty()) span += "font-family:" + s.font + ";";
        if (!span.empty()) span.pop_back();

        if (!span.empty()) out += "<span style=\"" + span + "\">";
        if (s.underline) out += "<u>";
        if (s.superscript) out += "<sup>";
        else if (s.subscript) out += "<sub>";

        for (size_t k = g.begin; k < g.coreBegin; ++k) {
            if (positions) (*positions)[k] = out.size();
            out += units[k].raw;
        }
        if (hasCore) {
            if (bold != s.bold) {
                out += "**";
                bold = s.bold;
            }
            if (italic != s.italic) {
                out += "*";
                italic = s.italic;
            }
            if (s.strike) out += "~~";
            for (size_t k = g.coreBegin; k < g.coreEnd; ++k) {
                if (positions) (*positions)[k] = out.size();
                out += units[k].raw;
            }
            if (s.strike) out += "~~";

            // Was braucht das naechste Stueck mit Inhalt? Alles andere wird
            // hier - vor den Leerzeichen - geschlossen.
            bool nextBold = false, nextItalic = false;
            for (size_t gj = gi + 1; gj < groups.size(); ++gj) {
                if (groups[gj].coreBegin < groups[gj].coreEnd) {
                    nextBold = units[groups[gj].begin].style.bold;
                    nextItalic = units[groups[gj].begin].style.italic;
                    break;
                }
            }
            if (italic && !nextItalic) {
                out += "*";
                italic = false;
            }
            if (bold && !nextBold) {
                out += "**";
                bold = false;
            }
        }
        for (size_t k = hasCore ? g.coreEnd : g.end; k < g.end; ++k) {
            if (positions) (*positions)[k] = out.size();
            out += units[k].raw;
        }

        if (s.superscript) out += "</sup>";
        else if (s.subscript) out += "</sub>";
        if (s.underline) out += "</u>";
        if (!span.empty()) out += "</span>";
    }
    return out;
}

// ---------------------------------------------------------------- Ausgabe
std::string renderManuscript(const Project& p, const std::string& text, bool keepFormatting) {
    std::string out;
    for (const ManuscriptToken& t : parseManuscript(p, text)) {
        switch (t.kind) {
            case ManuscriptToken::Kind::Text:
                out += t.raw;
                break;
            case ManuscriptToken::Kind::Markup:
                // Beim Export bleiben die Formatzeichen stehen - Obsidian und
                // der Word-Export verstehen sie.
                if (keepFormatting) out += t.raw;
                break;
            case ManuscriptToken::Kind::Heading:
                out += t.raw;
                break;
            case ManuscriptToken::Kind::Break:
                out += keepFormatting ? "---" : "* * *";
                break;
            case ManuscriptToken::Kind::Element:
                out += t.resolved ? p.displayName(t.targetId) : t.raw;
                break;
            case ManuscriptToken::Kind::Value: {
                if (!t.resolved) {
                    out += t.raw;
                    break;
                }
                const std::string value = p.valueAt(t.targetId, t.field, t.time);
                out += value.empty() ? p.displayName(t.targetId) : value;
                break;
            }
            case ManuscriptToken::Kind::Time:
                // Die Zeitmarke steuert nur die Werte - im fertigen Text steht sie nicht.
                break;
            case ManuscriptToken::Kind::Action:
                // Nur organisatorisch: die Marke sagt, an welcher Stelle im Text
                // die Aktion passiert - im fertigen Text steht sie nicht.
                break;
            case ManuscriptToken::Kind::Bookmark:
            case ManuscriptToken::Kind::Note:
                // Arbeitsnotizen des Autors gehoeren nicht in die fertige Geschichte.
                break;
        }
    }
    return out;
}

std::vector<std::string> mentionedElements(const Project& p, const std::string& text) {
    std::vector<std::string> out;
    for (const ManuscriptToken& t : parseManuscript(p, text)) {
        if (!t.resolved) continue;
        if (t.kind != ManuscriptToken::Kind::Element && t.kind != ManuscriptToken::Kind::Value)
            continue;
        bool known = false;
        for (const std::string& id : out) {
            if (id == t.targetId) known = true;
        }
        if (!known) out.push_back(t.targetId);
    }
    return out;
}

long long timeAtOffset(const Project& p, const std::string& text, size_t offset) {
    long long time = 0;
    for (const ManuscriptToken& t : parseManuscript(p, text)) {
        if (t.begin >= offset) break;
        if (t.kind == ManuscriptToken::Kind::Time) time = t.time;
    }
    return time;
}

void paragraphAt(const std::string& text, size_t offset, size_t* begin, size_t* end) {
    if (offset > text.size()) offset = text.size();
    size_t b = offset;
    while (b > 0) {
        if (b >= 2 && text[b - 1] == '\n' && text[b - 2] == '\n') break;
        --b;
    }
    size_t e = offset;
    while (e < text.size()) {
        if (e + 1 < text.size() && text[e] == '\n' && text[e + 1] == '\n') break;
        ++e;
    }
    if (begin) *begin = b;
    if (end) *end = e;
}

size_t countWords(const std::string& text) {
    size_t words = 0;
    bool inWord = false;
    for (unsigned char c : text) {
        const bool space = c == ' ' || c == '\n' || c == '\t' || c == '\r';
        if (space) {
            inWord = false;
        } else if (!inWord) {
            inWord = true;
            ++words;
        }
    }
    return words;
}

std::string sanitizeMarkerText(const std::string& text) {
    std::string out;
    for (size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        if (c == '\n' || c == '\r') {
            out += ' ';
        } else if (c == '%' && i + 1 < text.size() && text[i + 1] == '%') {
            out += '%';
            ++i;
        } else {
            out += c;
        }
    }
    // Ein einzelnes '%' am Ende wuerde mit dem Schluss "%%" verschmelzen.
    while (!out.empty() && out.back() == '%') out.pop_back();
    return out;
}

std::vector<size_t> findAll(const std::string& haystack, const std::string& needle,
                            bool caseSensitive) {
    std::vector<size_t> hits;
    if (needle.empty() || needle.size() > haystack.size()) return hits;
    for (size_t i = 0; i + needle.size() <= haystack.size(); ++i) {
        bool same = true;
        for (size_t k = 0; k < needle.size() && same; ++k) {
            const char a = haystack[i + k];
            const char b = needle[k];
            same = caseSensitive ? a == b : lower(a) == lower(b);
        }
        if (same) {
            hits.push_back(i);
            i += needle.size() - 1;  // Treffer ueberlappen sich nicht
        }
    }
    return hits;
}

}  // namespace se
