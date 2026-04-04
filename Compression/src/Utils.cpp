#include "Utils.hpp"
#include "logging.hpp"
#include <filesystem>
#include <vector>
#include <string>
#include <sstream>

namespace fs = std::filesystem;

// mode (from config.json) can steer encoder choice, e.g. "windows", "pi_hw", "pi_sw"
std::string buildFFmpegCommand(const std::string& outputPath,
    int width,
    int height,
    int crf,
    const std::string& mode)
{
    std::ostringstream cmd;

#if defined(__arm__) || defined(__aarch64__)
    // Raspberry Pi / ARM
    if (mode == "pi_hw")
    {
        // Preferred: hardware encoder (ensure ffmpeg has h264_v4l2m2m)
        cmd << "ffmpeg -y "
            << "-f rawvideo -pix_fmt bgr24 "
            << "-s " << width << "x" << height << " "
            << "-r 15 "
            << "-i - "
            << "-c:v h264_v4l2m2m "
            << "-b:v 800k -maxrate 1M -bufsize 2M "
            << "-pix_fmt yuv420p "
            << "\"" << outputPath << "\"";
    }
    else
    {
        // Safe fallback: software x264 on Pi
        cmd << "ffmpeg -y "
            << "-f rawvideo -pix_fmt bgr24 "
            << "-s " << width << "x" << height << " "
            << "-r 15 "
            << "-i - "
            << "-c:v libx264 "
            << "-preset veryfast "
            << "-crf " << crf << " "
            << "-pix_fmt yuv420p "
            << "\"" << outputPath << "\"";
    }
#else
    // Desktop / non-ARM: software x264 with CRF
    cmd << "ffmpeg -y "
        << "-f rawvideo -pix_fmt bgr24 "
        << "-s " << width << "x" << height << " "
        << "-r 15 "
        << "-i - "
        << "-c:v libx264 "
        << "-preset veryfast "
        << "-crf " << crf << " "
        << "-pix_fmt yuv420p "
        << "\"" << outputPath << "\"";
#endif

    return cmd.str();
}

bool ensureDirectory(const std::string& path)
{
    try
    {
        if (!fs::exists(path))
        {
            fs::create_directories(path);
            logInfo("Created directory: " + path);
        }
        return true;
    }
    catch (const std::exception& e)
    {
        logError("Failed to create directory " + path + ": " + e.what());
        return false;
    }
}

std::vector<std::string> listVideoFiles(const std::string& folder)
{
    std::vector<std::string> files;

    if (!fs::exists(folder))
        return files;

    for (auto& p : fs::directory_iterator(folder))
    {
        if (!p.is_regular_file()) continue;

        std::string ext = p.path().extension().string();
        if (ext == ".mp4" || ext == ".mov" || ext == ".avi" || ext == ".mkv")
            files.push_back(p.path().string());
    }

    return files;
}

bool moveFile(const std::string& src, const std::string& dst)
{
    try
    {
        fs::rename(src, dst);
        return true;
    }
    catch (const std::exception& e)
    {
        logError("Failed to move file: " + src + " → " + dst + " (" + e.what() + ")");
        return false;
    }
}
