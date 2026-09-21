// Persistence: the obsidian vault is written and read exclusively here.
#pragma once

#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "core/Project.h"

namespace se::vault {

// Creates the folder skeleton plus metadata.json for a brand new project.
bool createVault(const std::string& path, const std::string& name, Project& p, std::string* err);

bool load(const std::string& path, Project& p, std::string* err);
bool saveAll(Project& p, std::string* err);

bool saveMetadata(const Project& p, std::string* err);
bool saveActions(const Project& p, std::string* err);
bool saveConnections(const Project& p, std::string* err);
bool saveElement(const Project& p, Element& el, std::string* err);
void deleteElementFile(const Project& p, const Element& el);
bool ensureGroupDirs(const Project& p, std::string* err);

std::string elementRelativePath(const Project& p, const Element& el);
std::string absolutePath(const Project& p, const std::string& relative);
std::string elementMarkdown(const Project& p, const Element& el);
// Parses the "## Felder" block back; used for reload/merge after an external edit.
bool parseElementMarkdown(const std::string& text, std::string* outName,
                          std::vector<std::pair<std::string, std::string>>* outFields,
                          std::string* outBody, std::string* outId, std::string* outGroupPath);

// Full in-memory snapshot, used by undo/redo.
nlohmann::json snapshot(const Project& p);
void restore(const nlohmann::json& j, Project& p);

}  // namespace se::vault
