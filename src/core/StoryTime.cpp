#include "core/StoryTime.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "core/Model.h"

namespace se {
namespace {

std::string toLower(const std::string& s) {
    std::string out = s;
    for (char& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

bool startsWithWord(const std::string& s, size_t pos, const char* word, size_t* outLen) {
    size_t n = 0;
    while (word[n]) ++n;
    if (pos + n > s.size()) return false;
    for (size_t i = 0; i < n; ++i) {
        if (s[pos + i] != word[i]) return false;
    }
    *outLen = n;
    return true;
}

}  // namespace

long long minutesPerUnit(TimeUnit u) {
    switch (u) {
        case TimeUnit::Minute: return 1;
        case TimeUnit::Hour: return kMinutesPerHour;
        case TimeUnit::Day: return kMinutesPerDay;
        case TimeUnit::Week: return 7 * kMinutesPerDay;
        case TimeUnit::Month: return kDaysPerMonth * kMinutesPerDay;
        case TimeUnit::Year: return kMonthsPerYear * kDaysPerMonth * kMinutesPerDay;
    }
    return 1;
}

const char* timeUnitLabel(TimeUnit u) {
    const bool en = englishTexts();
    switch (u) {
        case TimeUnit::Minute: return en ? "Minute" : "Minute";
        case TimeUnit::Hour: return en ? "Hour" : "Stunde";
        case TimeUnit::Day: return en ? "Day" : "Tag";
        case TimeUnit::Week: return en ? "Week" : "Woche";
        case TimeUnit::Month: return en ? "Month" : "Monat";
        case TimeUnit::Year: return en ? "Year" : "Jahr";
    }
    return en ? "Day" : "Tag";
}

const char* const* timeUnitLabels(int* count) {
    static const char* de[] = {"Minute", "Stunde", "Tag", "Woche", "Monat", "Jahr"};
    static const char* en[] = {"Minute", "Hour", "Day", "Week", "Month", "Year"};
    if (count) *count = 6;
    return englishTexts() ? en : de;
}

bool parseStoryTime(const std::string& text, long long* outMinutes) {
    std::string s = toLower(trim(text));
    if (s.empty()) return false;

    long long year = 1, month = 1, day = 1, hour = 0, minute = 0;
    bool any = false;
    bool sawClock = false;

    size_t i = 0;
    while (i < s.size()) {
        if (!std::isdigit(static_cast<unsigned char>(s[i])) && s[i] != '-') {
            // keyword or separator
            size_t len = 0;
            long long* target = nullptr;
            if (startsWithWord(s, i, "jahr", &len) || startsWithWord(s, i, "year", &len)) {
                target = &year;
            } else if (startsWithWord(s, i, "monat", &len) || startsWithWord(s, i, "month", &len)) {
                target = &month;
            } else if (startsWithWord(s, i, "tag", &len) || startsWithWord(s, i, "day", &len)) {
                target = &day;
            }
            if (target) {
                i += len;
                while (i < s.size() && !std::isdigit(static_cast<unsigned char>(s[i])) && s[i] != '-') ++i;
                if (i >= s.size()) return false;
                char* end = nullptr;
                long long v = std::strtoll(s.c_str() + i, &end, 10);
                if (end == s.c_str() + i) return false;
                *target = v;
                any = true;
                i = static_cast<size_t>(end - s.c_str());
                continue;
            }
            ++i;
            continue;
        }

        // a bare number: either "HH:MM" or the day when nothing else claimed it
        char* end = nullptr;
        long long v = std::strtoll(s.c_str() + i, &end, 10);
        size_t next = static_cast<size_t>(end - s.c_str());
        if (next < s.size() && s[next] == ':') {
            hour = v;
            char* end2 = nullptr;
            minute = std::strtoll(s.c_str() + next + 1, &end2, 10);
            if (end2 == s.c_str() + next + 1) return false;
            i = static_cast<size_t>(end2 - s.c_str());
            sawClock = true;
            any = true;
        } else {
            if (!any) {
                day = v;
                any = true;
            }
            i = next;
        }
    }

    if (!any && !sawClock) return false;
    if (month < 1) month = 1;
    if (day < 1) day = 1;

    long long days = (year - 1) * kMonthsPerYear * kDaysPerMonth + (month - 1) * kDaysPerMonth + (day - 1);
    *outMinutes = days * kMinutesPerDay + hour * kMinutesPerHour + minute;
    return true;
}

std::string formatStoryTime(long long minutes) {
    long long d = dayOf(minutes);
    long long rest = minutes - (d - 1) * kMinutesPerDay;
    char buf[64];
    std::snprintf(buf, sizeof(buf), englishTexts() ? "Day %lld, %02lld:%02lld" : "Tag %lld, %02lld:%02lld",
                  d, rest / 60, rest % 60);
    return buf;
}

std::string formatStoryTimeLong(long long minutes) {
    long long dayIndex = minutes >= 0 ? minutes / kMinutesPerDay : 0;
    long long rest = minutes - dayIndex * kMinutesPerDay;
    long long year = dayIndex / (kMonthsPerYear * kDaysPerMonth);
    long long remDays = dayIndex - year * kMonthsPerYear * kDaysPerMonth;
    long long month = remDays / kDaysPerMonth;
    long long day = remDays - month * kDaysPerMonth;
    const bool en = englishTexts();
    char buf[96];
    if (year == 0 && month == 0) {
        std::snprintf(buf, sizeof(buf), en ? "Day %lld, %02lld:%02lld" : "Tag %lld, %02lld:%02lld",
                      day + 1, rest / 60, rest % 60);
    } else {
        std::snprintf(buf, sizeof(buf),
                      en ? "Year %lld, month %lld, day %lld, %02lld:%02lld"
                         : "Jahr %lld, Monat %lld, Tag %lld, %02lld:%02lld",
                      year + 1, month + 1, day + 1, rest / 60, rest % 60);
    }
    return buf;
}

std::string formatDayHeadline(long long minutes) {
    char buf[48];
    std::snprintf(buf, sizeof(buf), englishTexts() ? "DAY %lld" : "TAG %lld", dayOf(minutes));
    return buf;
}

std::string formatDuration(long long minutes) {
    if (minutes < 0) minutes = -minutes;
    const bool en = englishTexts();
    char buf[64];
    if (minutes < kMinutesPerHour) {
        std::snprintf(buf, sizeof(buf), en ? "%lld minute%s" : "%lld Minute%s", minutes,
                      minutes == 1 ? "" : (en ? "s" : "n"));
    } else if (minutes < kMinutesPerDay) {
        long long h = minutes / kMinutesPerHour;
        std::snprintf(buf, sizeof(buf), en ? "%lld hour%s" : "%lld Stunde%s", h,
                      h == 1 ? "" : (en ? "s" : "n"));
    } else if (minutes < 14 * kMinutesPerDay) {
        long long d = minutes / kMinutesPerDay;
        std::snprintf(buf, sizeof(buf), en ? "%lld day%s" : "%lld Tag%s", d,
                      d == 1 ? "" : (en ? "s" : "e"));
    } else if (minutes < kDaysPerMonth * 3 * kMinutesPerDay) {
        long long w = minutes / (7 * kMinutesPerDay);
        std::snprintf(buf, sizeof(buf), en ? "%lld week%s" : "%lld Woche%s", w,
                      w == 1 ? "" : (en ? "s" : "n"));
    } else if (minutes < kMonthsPerYear * kDaysPerMonth * kMinutesPerDay) {
        long long m = minutes / (kDaysPerMonth * kMinutesPerDay);
        std::snprintf(buf, sizeof(buf), en ? "%lld month%s" : "%lld Monat%s", m,
                      m == 1 ? "" : (en ? "s" : "e"));
    } else {
        long long y = minutes / (kMonthsPerYear * kDaysPerMonth * kMinutesPerDay);
        std::snprintf(buf, sizeof(buf), en ? "%lld year%s" : "%lld Jahr%s", y,
                      y == 1 ? "" : (en ? "s" : "e"));
    }
    return buf;
}

long long dayOf(long long minutes) {
    if (minutes < 0) minutes = 0;
    return minutes / kMinutesPerDay + 1;
}

long long startOfDay(long long minutes) {
    if (minutes < 0) minutes = 0;
    return (minutes / kMinutesPerDay) * kMinutesPerDay;
}

}  // namespace se
