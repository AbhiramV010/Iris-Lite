#include "file_watcher.hpp"
#include <filesystem>

// Looks through the folder and returns all .mp4 files.
// The compressor handles everything after that.

std::vector<std::string> FileWatcher::getNewVideos(const std::string& folderPath) {
    std::vector<std::string> results;

    if (!std::filesystem::exists(folderPath)) {
        return results;
    }

    for (const auto& entry : std::filesystem::directory_iterator(folderPath)) {
        if (entry.is_regular_file()) {
            if (entry.path().extension() == ".mp4") {
                results.push_back(entry.path().filename().string());
            }
        }
    }

    return results;
}
