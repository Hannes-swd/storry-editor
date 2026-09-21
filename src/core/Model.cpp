#include "core/Model.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <sstream>

#include <nlohmann/json.hpp>

namespace se {

const char* fieldTypeName(FieldType t) {
    switch (t) {
        case FieldType::Text: return "Text";
        case FieldType::Integer: return "Integer";
        case FieldType::Float: return "Float";
        case FieldType::Date: return "Date";
        case FieldType::Enum: return "Enum";
        case FieldType::Boolean: return "Boolean";
        case FieldType::List: return "List";
        case FieldType::Reference: return "Reference";
        case FieldType::File: return "File";
    }
    return "Text";
}

const char* fieldTypeLabel(FieldType t) {
    switch (t) {
        case FieldType::Text: return "Text";
        case FieldType::Integer: return "Integer (Ganzzahl)";
        case FieldType::Float: return "Float (Dezimalzahl)";
        case FieldType::Date: return "Date (Zeitpunkt)";
        case FieldType::Enum: return "Enum (Auswahl)";
        case FieldType::Boolean: return "Boolean (Ja/Nein)";
        case FieldType::List: return "List (Liste)";
        case FieldType::Reference: return "Reference (Verweis)";
        case FieldType::File: return "File (Datei)";
    }
    return "Text";
}

FieldType fieldTypeFromName(const std::string& s) {
    for (int i = 0; i <= static_cast<int>(FieldType::File); ++i) {
        FieldType t = static_cast<FieldType>(i);
        if (s == fieldTypeName(t)) return t;
    }
    return FieldType::Text;
}

const char* const* fieldTypeLabels(int* count) {
    static const char* labels[] = {
        "Text", "Integer (Ganzzahl)", "Float (Dezimalzahl)", "Date (Zeitpunkt)",
        "Enum (Auswahl)", "Boolean (Ja/Nein)", "List (Liste)", "Reference (Verweis)",
        "File (Datei)",
    };
    if (count) *count = static_cast<int>(sizeof(labels) / sizeof(labels[0]));
    return labels;
}

std::string newId(const char* prefix) {
    static std::atomic<unsigned long long> counter{0};
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    unsigned long long stamp =
        static_cast<unsigned long long>(std::chrono::duration_cast<std::chrono::microseconds>(now).count());
    std::ostringstream os;
    os << prefix << '_' << std::hex << stamp << '_' << counter.fetch_add(1);
    return os.str();
}

std::string joinList(const std::vector<std::string>& v, const char* sep) {
    std::string out;
    for (size_t i = 0; i < v.size(); ++i) {
        if (i) out += sep;
        out += v[i];
    }
    return out;
}

std::vector<std::string> splitString(const std::string& s, char sep) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == sep) {
            out.push_back(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    out.push_back(cur);
    return out;
}

std::string trim(const std::string& s) {
    size_t b = 0, e = s.size();
    auto isSpace = [](unsigned char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };
    while (b < e && isSpace(static_cast<unsigned char>(s[b]))) ++b;
    while (e > b && isSpace(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(b, e - b);
}

bool iequalsContains(const std::string& haystack, const std::string& needle) {
    if (needle.empty()) return true;
    if (needle.size() > haystack.size()) return false;
    auto lower = [](unsigned char c) { return static_cast<char>(std::tolower(c)); };
    for (size_t i = 0; i + needle.size() <= haystack.size(); ++i) {
        size_t j = 0;
        for (; j < needle.size(); ++j) {
            if (lower(static_cast<unsigned char>(haystack[i + j])) !=
                lower(static_cast<unsigned char>(needle[j])))
                break;
        }
        if (j == needle.size()) return true;
    }
    return false;
}

std::vector<std::string> listFromValue(const std::string& value) {
    std::vector<std::string> out;
    std::string v = trim(value);
    if (v.empty()) return out;
    if (v.front() == '[') {
        try {
            nlohmann::json j = nlohmann::json::parse(v);
            if (j.is_array()) {
                for (auto& item : j) {
                    out.push_back(item.is_string() ? item.get<std::string>() : item.dump());
                }
                return out;
            }
        } catch (...) {
            // fall through to comma separated parsing
        }
    }
    for (auto& part : splitString(v, ',')) {
        std::string t = trim(part);
        if (!t.empty()) out.push_back(t);
    }
    return out;
}

std::string listToValue(const std::vector<std::string>& items) {
    nlohmann::json j = nlohmann::json::array();
    for (auto& s : items) j.push_back(s);
    return j.dump();
}

std::string defaultValueFor(const FieldDef& f) {
    if (!f.defaultValue.empty()) return f.defaultValue;
    switch (f.type) {
        case FieldType::Integer: return "0";
        case FieldType::Float: return "0.0";
        case FieldType::Boolean: return "false";
        case FieldType::List: return "[]";
        case FieldType::Enum: return f.enumOptions.empty() ? std::string() : f.enumOptions.front();
        default: return std::string();
    }
}

}  // namespace se
