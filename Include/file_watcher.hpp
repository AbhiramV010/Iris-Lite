#pragma once
#include <string>
#include <vector>

// This class just checks a folder for .mp4 files.
// It's a small helper so main.cpp doesn't get messy.

class FileWatcher {
public:
    std::vector<std::string> getNewVideos(const std::string& folderPath);
};
