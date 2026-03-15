#include "utils.hpp"
#include <filesystem>

// Simple check to see if a file exists.
bool Utils::fileExists(const std::string& path) {
    return std::filesystem::exists(path);
}

// Replace the extension of a filename.
// Example: "clip.mp4" + "_compressed.mp4" → "clip_compressed.mp4"
std::string Utils::replaceExtension(const std::string& path, const std::string& newExt) {
    size_t dotPos = path.find_last_of('.');
    if (dotPos == std::string::npos) {
        // If there's no extension, just append the new one
        return path + newExt;
    }

    std::string base = path.substr(0, dotPos);
    return base + newExt;
}
