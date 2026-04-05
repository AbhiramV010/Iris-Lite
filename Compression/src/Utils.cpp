#include "Utils.hpp"
#include "logging.hpp"
#include <filesystem>
#include <vector>
#include <string>
#include <sstream>

namespace fs = std::filesystem;

std::string buildFFmpegCommand(const std::string& outputPath,
    int width,
    int height,
    int crf,
    const std::string& mode)
{
    std::ostringstream cmd;

#if defined(__arm__) || defined(__aarch64__)
    // ARM platform: attempt hardware encoding first.
    if (mode == "pi_hw")
    {
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
        // ARM fallback: software H.264 encoder.
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
    // Desktop platforms: use software encoder with quality control.
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
            logInfo("Created output directory: " + path);
        }
        return true;
    }
    catch (const std::exception& e)
    {
        logError("Directory creation failed for " + path + ": " + e.what());
        return false;
    }
}

std::vector<std::string> listVideoFiles(const std::string& folder)
{
    std::vector<std::string> files;

    if (!fs::exists(folder))
        return files;

    // Enumerate video files with supported extensions.
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
        logError("File transfer failed from " + src + " to " + dst + ": " + e.what());
        return false;
    }
}