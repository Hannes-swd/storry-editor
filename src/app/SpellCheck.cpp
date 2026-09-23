#include "app/SpellCheck.h"

#include <windows.h>
#include <objbase.h>
#include <spellcheck.h>

#include <algorithm>
#include <set>

#include "app/Platform.h"

namespace se::spell {
namespace {

struct State {
    bool comReady = false;
    ISpellCheckerFactory* factory = nullptr;
    ISpellChecker* checker = nullptr;
    std::string language;
    std::set<std::string> dictionary;  // dauerhaft (Datei)
    std::set<std::string> session;     // "Alle ignorieren"
    std::set<std::string> names;       // Elementnamen des Projekts
    bool dictionaryLoaded = false;
    uint64_t generation = 1;
};

State& state() {
    static State s;
    return s;
}

std::string dictionaryPath() { return platform::appConfigDir() + "/woerterbuch.txt"; }

void loadDictionary() {
    State& s = state();
    if (s.dictionaryLoaded) return;
    s.dictionaryLoaded = true;
    std::string text;
    if (!platform::readFile(dictionaryPath(), &text)) return;
    size_t start = 0;
    while (start < text.size()) {
        size_t end = text.find('\n', start);
        if (end == std::string::npos) end = text.size();
        std::string word = text.substr(start, end - start);
        while (!word.empty() && (word.back() == '\r' || word.back() == ' ')) word.pop_back();
        if (!word.empty()) s.dictionary.insert(word);
        start = end + 1;
    }
}

bool ensureFactory() {
    State& s = state();
    if (s.factory) return true;
    if (!s.comReady) {
        // Schon anders initialisiert (RPC_E_CHANGED_MODE) ist ebenfalls in Ordnung.
        CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        s.comReady = true;
    }
    return SUCCEEDED(CoCreateInstance(__uuidof(SpellCheckerFactory), nullptr, CLSCTX_INPROC_SERVER,
                                      IID_PPV_ARGS(&s.factory))) &&
           s.factory;
}

void ignoreAll(const std::set<std::string>& words) {
    State& s = state();
    if (!s.checker) return;
    for (const std::string& w : words) {
        const std::wstring wide = platform::toWide(w);
        if (!wide.empty()) s.checker->Ignore(wide.c_str());
    }
}

// UTF-8 -> UTF-16 mit Zuordnung jeder UTF-16-Stelle zum Byte-Offset.
std::wstring widen(const std::string& utf8, std::vector<size_t>* toByte) {
    std::wstring out;
    toByte->clear();
    size_t i = 0;
    while (i < utf8.size()) {
        const unsigned char c = static_cast<unsigned char>(utf8[i]);
        size_t n = c < 0x80 ? 1 : (c >> 5) == 0x6 ? 2 : (c >> 4) == 0xE ? 3 : (c >> 3) == 0x1E ? 4 : 1;
        n = std::min(n, utf8.size() - i);
        unsigned int cp = c;
        if (n == 2) cp = ((c & 0x1Fu) << 6) | (utf8[i + 1] & 0x3Fu);
        if (n == 3) cp = ((c & 0x0Fu) << 12) | ((utf8[i + 1] & 0x3Fu) << 6) | (utf8[i + 2] & 0x3Fu);
        if (n == 4)
            cp = ((c & 0x07u) << 18) | ((utf8[i + 1] & 0x3Fu) << 12) | ((utf8[i + 2] & 0x3Fu) << 6) |
                 (utf8[i + 3] & 0x3Fu);
        if (cp >= 0x10000) {
            cp -= 0x10000;
            out.push_back(static_cast<wchar_t>(0xD800 + (cp >> 10)));
            toByte->push_back(i);
            out.push_back(static_cast<wchar_t>(0xDC00 + (cp & 0x3FF)));
            toByte->push_back(i);
        } else {
            out.push_back(static_cast<wchar_t>(cp));
            toByte->push_back(i);
        }
        i += n;
    }
    toByte->push_back(utf8.size());
    return out;
}

}  // namespace

bool supported(const std::string& tag) {
    if (!ensureFactory()) return false;
    BOOL ok = FALSE;
    const std::wstring wide = platform::toWide(tag);
    return SUCCEEDED(state().factory->IsSupported(wide.c_str(), &ok)) && ok;
}

std::vector<std::string> installedLanguages() {
    std::vector<std::string> out;
    if (!ensureFactory()) return out;
    IEnumString* list = nullptr;
    if (FAILED(state().factory->get_SupportedLanguages(&list)) || !list) return out;
    LPOLESTR item = nullptr;
    while (list->Next(1, &item, nullptr) == S_OK && item) {
        out.push_back(platform::toUtf8(item));
        CoTaskMemFree(item);
        item = nullptr;
    }
    list->Release();
    return out;
}

bool setLanguage(const std::string& tag) {
    State& s = state();
    if (s.language == tag && s.checker) return true;
    loadDictionary();
    if (s.checker) {
        s.checker->Release();
        s.checker = nullptr;
    }
    s.language = tag;
    ++s.generation;
    if (!supported(tag)) return false;
    const std::wstring wide = platform::toWide(tag);
    if (FAILED(s.factory->CreateSpellChecker(wide.c_str(), &s.checker))) {
        s.checker = nullptr;
        return false;
    }
    // Eigene Woerter gelten in jeder Sprache.
    ignoreAll(s.dictionary);
    ignoreAll(s.session);
    ignoreAll(s.names);
    return true;
}

const std::string& language() { return state().language; }
bool available() { return state().checker != nullptr; }
uint64_t generation() { return state().generation; }

std::vector<Issue> check(const std::string& utf8) {
    std::vector<Issue> issues;
    State& s = state();
    if (!s.checker || utf8.empty()) return issues;
    std::vector<size_t> toByte;
    const std::wstring wide = widen(utf8, &toByte);
    IEnumSpellingError* errors = nullptr;
    if (FAILED(s.checker->Check(wide.c_str(), &errors)) || !errors) return issues;
    ISpellingError* err = nullptr;
    while (errors->Next(&err) == S_OK && err) {
        ULONG start = 0, length = 0;
        err->get_StartIndex(&start);
        err->get_Length(&length);
        CORRECTIVE_ACTION action = CORRECTIVE_ACTION_NONE;
        err->get_CorrectiveAction(&action);
        Issue issue;
        issue.begin = toByte[std::min<size_t>(start, toByte.size() - 1)];
        issue.end = toByte[std::min<size_t>(start + length, toByte.size() - 1)];
        if (action == CORRECTIVE_ACTION_REPLACE) {
            LPWSTR replacement = nullptr;
            if (SUCCEEDED(err->get_Replacement(&replacement)) && replacement) {
                issue.replacement = platform::toUtf8(replacement);
                CoTaskMemFree(replacement);
            }
        }
        if (issue.end > issue.begin) issues.push_back(issue);
        err->Release();
        err = nullptr;
    }
    errors->Release();
    return issues;
}

bool isCorrect(const std::string& word) {
    if (!available() || word.empty()) return true;
    for (const Issue& i : check(word)) {
        if (i.begin == 0 && i.end >= word.size()) return false;
    }
    return true;
}

std::vector<std::string> suggest(const std::string& word, size_t max) {
    std::vector<std::string> out;
    State& s = state();
    if (!s.checker || word.empty()) return out;
    IEnumString* list = nullptr;
    const std::wstring wide = platform::toWide(word);
    if (FAILED(s.checker->Suggest(wide.c_str(), &list)) || !list) return out;
    LPOLESTR item = nullptr;
    while (out.size() < max && list->Next(1, &item, nullptr) == S_OK && item) {
        out.push_back(platform::toUtf8(item));
        CoTaskMemFree(item);
        item = nullptr;
    }
    list->Release();
    return out;
}

std::string autoCorrection(const std::string& word) {
    for (const Issue& i : check(word)) {
        if (i.begin == 0 && i.end >= word.size() && !i.replacement.empty()) return i.replacement;
    }
    return std::string();
}

void addToDictionary(const std::string& word) {
    State& s = state();
    loadDictionary();
    if (word.empty() || !s.dictionary.insert(word).second) return;
    std::string text;
    for (const std::string& w : s.dictionary) text += w + "\n";
    std::string err;
    platform::writeFile(dictionaryPath(), text, &err);
    ignoreAll({word});
    ++s.generation;
}

void ignoreForSession(const std::string& word) {
    State& s = state();
    if (word.empty() || !s.session.insert(word).second) return;
    ignoreAll({word});
    ++s.generation;
}

void setKnownNames(const std::vector<std::string>& names) {
    State& s = state();
    std::set<std::string> fresh;
    for (const std::string& name : names) {
        // "Alte Muehle" -> beide Woerter gelten als richtig
        size_t start = 0;
        while (start <= name.size()) {
            size_t end = name.find(' ', start);
            if (end == std::string::npos) end = name.size();
            if (end > start) fresh.insert(name.substr(start, end - start));
            start = end + 1;
        }
    }
    bool added = false;
    for (const std::string& n : fresh) {
        if (s.names.insert(n).second) added = true;
    }
    if (added) {
        ignoreAll(s.names);
        ++s.generation;
    }
}

}  // namespace se::spell
