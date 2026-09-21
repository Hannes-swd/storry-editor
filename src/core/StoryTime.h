// Story time is stored as an absolute amount of minutes since the start of the
// story. Day 1, 00:00 == 0. The fictional calendar uses fixed 30 day months and
// 12 month (360 day) years so that relative maths stays predictable.
#pragma once

#include <string>

namespace se {

enum class TimeUnit { Minute, Hour, Day, Week, Month, Year };

constexpr long long kMinutesPerHour = 60;
constexpr long long kMinutesPerDay = 24 * 60;
constexpr long long kDaysPerMonth = 30;
constexpr long long kMonthsPerYear = 12;

long long minutesPerUnit(TimeUnit u);
const char* timeUnitLabel(TimeUnit u);
const char* const* timeUnitLabels(int* count);

// "Tag 5, 14:30" / "Day 5, 14:30" / "Jahr 2, Monat 3, Tag 15" / "5" / "14:30"
bool parseStoryTime(const std::string& text, long long* outMinutes);

// Canonical short form, e.g. "Tag 5, 14:30".
std::string formatStoryTime(long long minutes);
// Long form including year/month once the story runs longer than a month.
std::string formatStoryTimeLong(long long minutes);
// Day headline used by the story visualizer, e.g. "TAG 5".
std::string formatDayHeadline(long long minutes);
// "3 Tage", "2 Stunden", "5 Wochen" - used for gaps between events.
std::string formatDuration(long long minutes);

long long dayOf(long long minutes);   // 1 based day index
long long startOfDay(long long minutes);

}  // namespace se
