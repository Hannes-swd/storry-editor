// Headless checks for the data layer, run with "StoryEditor.exe --selftest".
#pragma once

#include <string>

namespace se {

// Returns 0 when every check passed. Writes a report to `reportPath` (if set).
int runSelfTest(const std::string& reportPath);

// Creates a small demo vault (Alice, Bob, Castle, Sword, a few actions) so that
// the windows can be tried out without typing everything in first.
int createDemoProject(const std::string& vaultPath);

}  // namespace se
