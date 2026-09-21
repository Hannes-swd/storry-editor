// Detects .md files that were changed outside of the editor (spec 2.2).
#pragma once

#include <map>
#include <string>
#include <vector>

#include "core/Project.h"

namespace se {

struct ExternalChange {
    std::string elementId;
    std::string relativePath;
};

class FileWatcher {
public:
    void reset(const Project& p);
    void touch(const Project& p, const Element& el);  // call after the editor wrote the file
    void forget(const std::string& relativePath);
    // Polls at most every `interval` seconds and reports files changed on disk.
    std::vector<ExternalChange> poll(const Project& p, float dtSeconds, float interval = 1.5f);

private:
    std::map<std::string, long long> stamps_;
    float accum_ = 0.0f;
};

}  // namespace se
