#include "core/Manuscript.h"

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

}  // namespace

std::vector<ManuscriptToken> parseManuscript(const Project& p, const std::string& text) {
    std::vector<ManuscriptToken> out;
    long long currentTime = 0;
    size_t textStart = 0;

    auto flushText = [&](size_t upTo) {
        if (upTo <= textStart) return;
        ManuscriptToken t;
        t.kind = ManuscriptToken::Kind::Text;
        t.begin = textStart;
        t.end = upTo;
        t.raw = text.substr(textStart, upTo - textStart);
        t.time = currentTime;
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
            i = lineEnd;
            textStart = i;
            continue;
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
                t.kind = field.empty() ? ManuscriptToken::Kind::Element : ManuscriptToken::Kind::Value;
                t.begin = i;
                t.end = j;
                t.raw = text.substr(i, j - i);
                t.targetId = resolveElement(p, reference);
                t.field = field;
                t.time = currentTime;
                t.resolved = !t.targetId.empty();
                out.push_back(t);
                i = j;
                textStart = i;
                continue;
            }
        }

        ++i;
    }
    flushText(text.size());
    return out;
}

std::string renderManuscript(const Project& p, const std::string& text) {
    std::string out;
    for (const ManuscriptToken& t : parseManuscript(p, text)) {
        switch (t.kind) {
            case ManuscriptToken::Kind::Text:
            case ManuscriptToken::Kind::Heading:
                out += t.raw;
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
            case ManuscriptToken::Kind::Action: {
                const Action* a = p.action(t.targetId);
                if (a)
                    out += a->title;
                else
                    out += t.raw;
                break;
            }
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

}  // namespace se
