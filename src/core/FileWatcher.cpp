#include "core/FileWatcher.h"

#include "app/Platform.h"
#include "core/VaultIO.h"

namespace se {

void FileWatcher::reset(const Project& p) {
    stamps_.clear();
    accum_ = 0.0f;
    for (const Element& el : p.elements) {
        if (el.filePath.empty()) continue;
        stamps_[el.filePath] = platform::lastWriteTime(vault::absolutePath(p, el.filePath));
    }
}

void FileWatcher::touch(const Project& p, const Element& el) {
    if (el.filePath.empty()) return;
    stamps_[el.filePath] = platform::lastWriteTime(vault::absolutePath(p, el.filePath));
}

void FileWatcher::forget(const std::string& relativePath) { stamps_.erase(relativePath); }

std::vector<ExternalChange> FileWatcher::poll(const Project& p, float dtSeconds, float interval) {
    std::vector<ExternalChange> changes;
    accum_ += dtSeconds;
    if (accum_ < interval || p.vaultPath.empty()) return changes;
    accum_ = 0.0f;

    for (const Element& el : p.elements) {
        if (el.filePath.empty()) continue;
        long long stamp = platform::lastWriteTime(vault::absolutePath(p, el.filePath));
        auto it = stamps_.find(el.filePath);
        if (it == stamps_.end()) {
            stamps_[el.filePath] = stamp;
            continue;
        }
        if (stamp != 0 && stamp != it->second) {
            it->second = stamp;
            changes.push_back({el.id, el.filePath});
        }
    }
    return changes;
}

}  // namespace se
