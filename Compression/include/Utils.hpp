#pragma once
#include <string>
#include <mutex>
#include <opencv2/core.hpp>
#pragma once
#include <string>

std::string buildFFmpegCommand(const std::string& outputPath,
    int width,
    int height,
    int crf,
    const std::string& mode);

// Logging severity levels
enum class LogLevel
{
    INFO,
    WARN,
    ERROR,
    DEBUG
};



// Time helpers
double parseTimestamp(const std::string& ts);   // "HH:MM:SS" or "12.5"
std::string currentTimestampString();

// Math helpers
template<typename T>
T clamp(T v, T lo, T hi)
{
    return (v < lo) ? lo : (v > hi ? hi : v);
}

// FFmpeg command builder
std::string buildFFmpegCommand(
    const std::string& outputPath,
    int width,
    int height,
    int crf,
    const std::string& mode
);
bool ensureDirectory(const std::string& path);
std::vector<std::string> listVideoFiles(const std::string& folder);
bool moveFile(const std::string& src, const std::string& dst);

