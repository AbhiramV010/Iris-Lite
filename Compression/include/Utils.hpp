#pragma once

#include <string>
#include <vector>
#include <algorithm>
// FFmpeg command builder (platform + mode aware)
std::string buildFFmpegCommand(
    const std::string& outputPath,
    int width,
    int height,
    int crf,
    const std::string& mode
);

// Time helpers (declared; optional to implement)
double parseTimestamp(const std::string& ts);   // "HH:MM:SS" or "12.5"
std::string currentTimestampString();

// Math helpers
template<typename T>
T clamp(T v, T lo, T hi)
{
    return (v < lo) ? lo : (v > hi ? hi : v);
}

// Filesystem helpers
bool ensureDirectory(const std::string& path);
std::vector<std::string> listVideoFiles(const std::string& folder);
bool moveFile(const std::string& src, const std::string& dst);
