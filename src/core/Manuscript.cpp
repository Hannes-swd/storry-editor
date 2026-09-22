#include "core/Manuscript.h"

#include <algorithm>
#include <cctype>

#include "core/StoryTime.h"

namespace se {
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

// Ende des Absatzes, in dem `from` liegt. Hervorhebungen gelten nie ueber eine
// Leerzeile hinweg - so faerbt ein vergessenes Sternchen nicht den Rest des
// Buches ein.
size_t paragraphEnd(const std::string& text, size_t from) {
    size_t i = from;
    while (i + 1 < text.size()) {
        if (text[i] == '\n' && text[i + 1] == '\n') return i;
        ++i;
    }
    return text.size();
}

// Gibt es im selben Absatz ein passendes Schlusszeichen? Nur dann ist das
// Sternchen eine Hervorhebung und kein normales Satzzeichen.
bool hasClosingMark(const std::string& text, size_t after, size_t markLength) {
    const size_t stop = paragraphEnd(text, after);
    for (size_t i = after; i + markLength <= stop; ++i) {
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

}  // namespace

std::vector<ManuscriptToken> parseManuscript(const Project& p, const std::string& text) {
    std::vector<ManuscriptToken> out;
    long long currentTime = 0;
    size_t textStart = 0;
    bool bold = false;
    bool italic = false;

    auto flushText = [&](size_t upTo) {
        if (upTo <= textStart) return;
        ManuscriptToken t;
        t.kind = ManuscriptToken::Kind::Text;
        t.begin = textStart;
        t.end = upTo;
        t.raw = text.substr(textStart, upTo - textStart);
        t.time = currentTime;
        t.bold = bold;
        t.italic = italic;
        out.push_back(t);
    };

    size_t i = 0;
    while (i < text.size()) {
        const char c = text[i];

        // ---------------------------------------------------- Ueberschrift
        if (c == '#' && atLineStart(text, i) && i + 1 < text.size() &&
            (text[i + 1] == '#' || text[i + 1] == ' ')) {
            size_t lineEnd = text.find('\n', i);
            if (lineEnd == std::string::npos) lineEnd = text.size();
            flushText(i);
            ManuscriptToken t;
            t.kind = ManuscriptToken::Kind::Heading;
            t.begin = i;
            t.end = lineEnd;
            t.raw = text.substr(i, lineEnd - i);
            t.time = currentTime;
            t.resolved = true;
            out.push_back(t);
            bold = italic = false;  // eine Ueberschrift beendet jede Hervorhebung
            i = lineEnd;
            textStart = i;
            continue;
        }

        // ---------------------------------------------------- Szenenwechsel
        if (c == '-' && atLineStart(text, i)) {
            size_t lineEnd = text.find('\n', i);
            if (lineEnd == std::string::npos) lineEnd = text.size();
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
                bold = italic = false;
                i = lineEnd;
                textStart = i;
                continue;
            }
        }

        // ---------------------------------------------------- Zeitmarke
        if (c == '#' && i + 1 < text.size() && text[i + 1] != '#' && text[i + 1] != ' ') {
            size_t lineEnd = text.find('\n', i);
            if (lineEnd == std::string::npos) lineEnd = text.size();
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
            out.push_back(t);
            i = j;
            textStart = i;
            continue;
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
                flag = !flag;
                i = after;
                textStart = i;
                continue;
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
                t.bold = bold;
                t.italic = italic;
                out.push_back(t);
                i = j;
                textStart = i;
                continue;
            }
        }

        // Eine Leerzeile beendet jede Hervorhebung - siehe paragraphEnd().
        if (c == '\n' && i + 1 < text.size() && text[i + 1] == '\n' && (bold || italic)) {
            flushText(i);
            bold = italic = false;
            textStart = i;
        }

        ++i;
    }
    flushText(text.size());
    return out;
}

std::string renderManuscript(const Project& p, const std::string& text, bool keepFormatting) {
    std::string out;
    bool bold = false;
    bool italic = false;

    // Beim Export bleiben die Formatzeichen stehen; dort werden sie in echte
    // Word-Formatierung uebersetzt.
    auto applyStyle = [&](const ManuscriptToken& t) {
        if (!keepFormatting) return;
        if (italic && !t.italic) {
            out += "*";
            italic = false;
        }
        if (bold != t.bold) {
            out += "**";
            bold = t.bold;
        }
        if (!italic && t.italic) {
            out += "*";
            italic = true;
        }
    };

    for (const ManuscriptToken& t : parseManuscript(p, text)) {
        switch (t.kind) {
            case ManuscriptToken::Kind::Text:
                applyStyle(t);
                out += t.raw;
                break;
            case ManuscriptToken::Kind::Heading:
                out += t.raw;
                break;
            case ManuscriptToken::Kind::Break:
                out += keepFormatting ? "---" : "* * *";
                break;
            case ManuscriptToken::Kind::Element:
                applyStyle(t);
                out += t.resolved ? p.displayName(t.targetId) : t.raw;
                break;
            case ManuscriptToken::Kind::Value: {
                applyStyle(t);
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
        }
    }
    if (keepFormatting) {
        if (italic) out += "*";
        if (bold) out += "**";
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
