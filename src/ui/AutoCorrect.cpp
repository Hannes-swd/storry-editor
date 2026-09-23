#include "ui/AutoCorrect.h"

#include <windows.h>

#include <algorithm>
#include <vector>

#include "app/SpellCheck.h"
#include "core/Manuscript.h"
#include "ui/DocumentView.h"

namespace se::autocorrect {
namespace {

// ------------------------------------------------------------------ UTF-8
size_t seqLen(unsigned char c) {
    if (c < 0x80) return 1;
    if ((c >> 5) == 0x6) return 2;
    if ((c >> 4) == 0xE) return 3;
    if ((c >> 3) == 0x1E) return 4;
    return 1;
}

unsigned int decodeAt(const std::string& s, size_t i, size_t* len) {
    const unsigned char c = static_cast<unsigned char>(s[i]);
    size_t n = std::min(seqLen(c), s.size() - i);
    unsigned int cp = c;
    if (n == 2) cp = ((c & 0x1Fu) << 6) | (s[i + 1] & 0x3Fu);
    if (n == 3) cp = ((c & 0x0Fu) << 12) | ((s[i + 1] & 0x3Fu) << 6) | (s[i + 2] & 0x3Fu);
    if (n == 4)
        cp = ((c & 0x07u) << 18) | ((s[i + 1] & 0x3Fu) << 12) | ((s[i + 2] & 0x3Fu) << 6) | (s[i + 3] & 0x3Fu);
    if (len) *len = n;
    return cp;
}

// Anfang des Zeichens vor `i`.
size_t prevStart(const std::string& s, size_t i) {
    if (i == 0) return 0;
    --i;
    while (i > 0 && (static_cast<unsigned char>(s[i]) & 0xC0) == 0x80) --i;
    return i;
}

std::string encode(unsigned int cp) {
    std::string out;
    if (cp < 0x80) {
        out += static_cast<char>(cp);
    } else if (cp < 0x800) {
        out += static_cast<char>(0xC0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        out += static_cast<char>(0xE0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
        out += static_cast<char>(0xF0 | (cp >> 18));
        out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
    return out;
}

// Buchstaben, Gross/Klein - ueber Windows, damit auch Umlaute stimmen.
bool isLetter(unsigned int cp) { return cp < 0x10000 && IsCharAlphaW(static_cast<wchar_t>(cp)); }
bool isUpper(unsigned int cp) { return cp < 0x10000 && IsCharUpperW(static_cast<wchar_t>(cp)); }
bool isLower(unsigned int cp) { return cp < 0x10000 && IsCharLowerW(static_cast<wchar_t>(cp)); }
unsigned int toUpper(unsigned int cp) {
    if (cp >= 0x10000 || cp == 0xDF) return cp;  // "ß" bleibt
    wchar_t w = static_cast<wchar_t>(cp);
    CharUpperBuffW(&w, 1);
    return w;
}
unsigned int toLower(unsigned int cp) {
    if (cp >= 0x10000) return cp;
    wchar_t w = static_cast<wchar_t>(cp);
    CharLowerBuffW(&w, 1);
    return w;
}

bool german(const std::string& language) { return language.rfind("de", 0) == 0; }

// Vorheriges *sichtbares* Zeichen: Formatzeichen (**, ~~, <u>...) zaehlen nicht.
unsigned int visibleBefore(const std::string& text, size_t pos, size_t* where) {
    size_t i = pos;
    while (i > 0) {
        const char c = text[i - 1];
        if (c == '*' || c == '~') {
            --i;
            continue;
        }
        if (c == '>') {
            const size_t open = text.rfind('<', i - 1);
            const size_t line = text.rfind('\n', i - 1);
            if (open != std::string::npos && (line == std::string::npos || open > line)) {
                i = open;
                continue;
            }
        }
        const size_t start = prevStart(text, i);
        if (where) *where = start;
        return decodeAt(text, start, nullptr);
    }
    if (where) *where = 0;
    return 0;
}

bool isOpeningContext(unsigned int prev) {
    return prev == 0 || prev == ' ' || prev == '\n' || prev == '\t' || prev == '(' || prev == '[' ||
           prev == '{' || prev == 0x2013 || prev == 0x2014 || prev == 0x201E || prev == 0x201C ||
           prev == 0x201A || prev == 0x2018 || prev == '/';
}

// Abkuerzungen, nach denen kein neuer Satz beginnt.
bool isAbbreviation(const std::string& word) {
    static const char* list[] = {"z.B", "d.h", "u.a", "usw", "bzw", "ca", "vgl", "Nr", "Dr", "Hr", "Fr",
                                 "Prof", "St", "evtl", "ggf", "inkl", "etc", "e.g", "i.e", "vs", "Mr",
                                 "Mrs", "Ms", "z", "u", "d", "s", "o.", "S"};
    for (const char* a : list) {
        if (word == a) return true;
    }
    // Initialen wie "J." oder "J.R."
    size_t letters = 0;
    for (char c : word) {
        if (c != '.') ++letters;
    }
    return letters == 1;
}

// Beginnt an `begin` ein neuer Satz?
bool sentenceStart(ManuscriptDoc& doc, size_t begin) {
    const std::string& text = doc.text();
    const std::vector<ManuscriptLine>& lines = doc.lines();
    const ManuscriptLine& L = lines[lineIndexAt(lines, begin)];
    size_t i = begin;
    while (i > L.contentBegin) {
        size_t at = 0;
        const unsigned int cp = visibleBefore(text, i, &at);
        if (at < L.contentBegin || cp == 0) break;
        if (cp == ' ' || cp == '\t' || cp == '"' || cp == '(' || cp == 0x201E || cp == 0x201C ||
            cp == 0x201A || cp == 0x2018 || cp == 0x2013 || cp == 0x2014) {
            i = at;
            continue;
        }
        if (cp == '.' || cp == '!' || cp == '?' || cp == 0x2026) {
            if (cp != '.') return true;
            // Wort vor dem Punkt (samt inneren Punkten wie in "z.B.")
            size_t w = at;
            while (w > L.contentBegin && (text[w - 1] == '.' || isLetter(decodeAt(text, prevStart(text, w), nullptr))))
                w = prevStart(text, w);
            return !isAbbreviation(text.substr(w, at - w));
        }
        return false;
    }
    return true;  // Anfang des Absatzes
}

// Wort, das bei `end` aufhoert: nur Buchstaben.
size_t wordBegin(const std::string& text, size_t end) {
    size_t b = end;
    while (b > 0) {
        const size_t p = prevStart(text, b);
        if (!isLetter(decodeAt(text, p, nullptr))) break;
        b = p;
    }
    return b;
}

std::vector<unsigned int> codepoints(const std::string& s) {
    std::vector<unsigned int> out;
    size_t i = 0;
    while (i < s.size()) {
        size_t n = 1;
        out.push_back(decodeAt(s, i, &n));
        i += n;
    }
    return out;
}

std::string join(const std::vector<unsigned int>& cps) {
    std::string out;
    for (unsigned int cp : cps) out += encode(cp);
    return out;
}

// Wort vor `end` pruefen und ggf. ersetzen; der Cursor bleibt hinter dem Getippten.
void fixWord(ManuscriptDoc& doc, DocumentView& view, size_t end, const std::string& language) {
    const std::string& text = doc.text();
    const size_t begin = wordBegin(text, end);
    if (begin >= end) return;
    // Teil einer Marke (@Alice, #Tag, !act:, Tags) - nicht anfassen
    if (begin > 0) {
        const char before = text[begin - 1];
        if (before == '@' || before == '#' || before == '!' || before == ':' || before == '/' || before == '<' ||
            before == '.' || before == '%' || before == '_' || (before >= '0' && before <= '9'))
            return;
    }
    if (end < text.size() && (text[end] == '.' && end + 1 < text.size() && text[end + 1] != ' ')) return;

    const std::string word = text.substr(begin, end - begin);
    std::string fixed = word;

    const std::string corrected = spell::autoCorrection(word);
    if (!corrected.empty()) fixed = corrected;

    std::vector<unsigned int> cps = codepoints(fixed);
    // ZWei GRosse Anfangsbuchstaben
    if (cps.size() >= 3 && isUpper(cps[0]) && isUpper(cps[1])) {
        bool restLower = true;
        for (size_t k = 2; k < cps.size(); ++k) {
            if (!isLower(cps[k])) restLower = false;
        }
        if (restLower) cps[1] = toLower(cps[1]);
    }
    // Satzanfang gross
    if (!cps.empty() && isLower(cps[0]) && sentenceStart(doc, begin)) cps[0] = toUpper(cps[0]);
    fixed = join(cps);
    (void)language;

    if (fixed == word) return;
    const long long delta = static_cast<long long>(fixed.size()) - static_cast<long long>(word.size());
    const size_t caret = static_cast<size_t>(static_cast<long long>(view.cursor) + delta);
    doc.replace(begin, end - begin, fixed, &view, caret, caret);
}

}  // namespace

std::string transformTyped(const std::string& text, size_t cursor, const std::string& typed,
                           const std::string& language) {
    const bool de = german(language);
    std::string out;
    size_t i = 0;
    while (i < typed.size()) {
        size_t n = 1;
        const unsigned int cp = decodeAt(typed, i, &n);
        if (cp == '"' || cp == '\'') {
            unsigned int prev = 0;
            if (!out.empty()) {
                prev = decodeAt(out, prevStart(out, out.size()), nullptr);
            } else {
                prev = visibleBefore(text, std::min(cursor, text.size()), nullptr);
            }
            const bool opening = isOpeningContext(prev);
            if (cp == '"')
                out += encode(opening ? (de ? 0x201E : 0x201C) : (de ? 0x201C : 0x201D));
            else
                out += encode(opening ? (de ? 0x201A : 0x2018) : 0x2019);
        } else {
            out += typed.substr(i, n);
        }
        i += n;
    }
    return out;
}

void afterTyped(ManuscriptDoc& doc, DocumentView& view, const std::string& typed, const std::string& language) {
    if (typed.empty() || view.hasSelection()) return;
    const char last = typed.back();
    const size_t cursor = view.cursor;
    const std::string& text = doc.text();
    if (cursor == 0 || cursor > text.size()) return;

    // "..." -> "…"
    if (last == '.' && cursor >= 3 && text.compare(cursor - 3, 3, "...") == 0) {
        const std::string ellipsis = "\xE2\x80\xA6";
        doc.replace(cursor - 3, 3, ellipsis, &view, cursor - 3 + ellipsis.size(), cursor - 3 + ellipsis.size());
        return;
    }
    // " - " bzw. " -- " zwischen Woertern -> Gedankenstrich
    if (last == ' ') {
        const std::string dash = "\xE2\x80\x93";
        if (cursor >= 4 && text.compare(cursor - 4, 4, " -- ") == 0 && cursor >= 5 && text[cursor - 5] != ' ') {
            doc.replace(cursor - 3, 2, dash, &view, cursor - 2 + dash.size(), cursor - 2 + dash.size());
            return;
        }
        if (cursor >= 3 && text.compare(cursor - 3, 3, " - ") == 0 && cursor >= 4 && text[cursor - 4] != ' ' &&
            text[cursor - 4] != '\n') {
            doc.replace(cursor - 2, 1, dash, &view, cursor - 1 + dash.size(), cursor - 1 + dash.size());
            return;
        }
    }
    // Wortende: Leerzeichen oder Satzzeichen
    static const std::string delimiters = " .,;:!?)]}\t";
    if (delimiters.find(last) != std::string::npos) fixWord(doc, view, cursor - 1, language);
}

void beforeParagraph(ManuscriptDoc& doc, DocumentView& view, const std::string& language) {
    if (view.hasSelection()) return;
    fixWord(doc, view, view.cursor, language);
}

}  // namespace se::autocorrect
