// Thin win32 layer: unicode path handling, native dialogs, shell helpers.
#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace se::platform {

std::wstring toWide(const std::string& utf8);
std::string toUtf8(const std::wstring& wide);

// std::filesystem on MSVC interprets narrow strings as ANSI - always build the
// path from a wide string so that umlauts survive.
std::filesystem::path fsPath(const std::string& utf8);
std::string pathToUtf8(const std::filesystem::path& p);

bool readFile(const std::string& path, std::string* out);
bool writeFile(const std::string& path, const std::string& content, std::string* err);
bool ensureDir(const std::string& path, std::string* err);
long long lastWriteTime(const std::string& path);  // 0 when missing

// Native dialogs (empty result == cancelled)
std::string pickFolder(const char* title);
std::string pickFile(const char* title, const char* filterLabel, const char* filterPattern);
std::vector<std::string> pickFiles(const char* title);
std::string saveFileDialog(const char* title, const char* defaultName);

void openInShell(const std::string& path);  // open file/folder with the default app

std::string appConfigDir();  // %APPDATA%/StoryEditor

}  // namespace se::platform
