#include "H264Encoder.hpp"
#include "logging.hpp"

#include <sstream>
#include <vector>

H264Encoder::H264Encoder(int width_, int height_, int fps_, int bitrate_, bool useHardware_)
    : width(width_),
    height(height_),
    fps(fps_),
    bitrate(bitrate_),
    useHardware(useHardware_)
{
}

H264Encoder::~H264Encoder()
{
    close();
}

// ---------------- OPEN ----------------

bool H264Encoder::open(const std::string& outputPath)
{
    ffmpegCommand = buildCommand(outputPath);

    logInfo("FFmpeg command: " + ffmpegCommand);

#ifdef _WIN32
    ffmpegPipe = _popen(ffmpegCommand.c_str(), "wb");
#else
    ffmpegPipe = popen(ffmpegCommand.c_str(), "w");
#endif

    if (!ffmpegPipe)
    {
        logError("Failed to open FFmpeg pipe");
        return false;
    }

    logInfo("Encoder started: " + outputPath);
    return true;
}

// ---------------- WRITE FRAME ----------------

bool H264Encoder::writeFrame(const cv::Mat& frame)
{
    if (!ffmpegPipe || frame.empty())
        return false;

    if (!ffmpegPipe)
    {
        logError("FFmpeg pipe is null (encoder not running)");
        return false;
    }

    size_t bytes = frame.total() * frame.elemSize();
    size_t written = fwrite(frame.data, 1, bytes, ffmpegPipe);

    if (written != bytes)
    {
        logError("FFmpeg write failed (partial write)");

#ifdef _WIN32
        _pclose(ffmpegPipe);
#else
        pclose(ffmpegPipe);
#endif

        ffmpegPipe = nullptr;
        return false;
    }

    return true;
}

// ---------------- CLOSE ----------------

void H264Encoder::close()
{
    if (!ffmpegPipe)
        return;

#ifdef _WIN32
    _pclose(ffmpegPipe);
#else
    pclose(ffmpegPipe);
#endif

    ffmpegPipe = nullptr;
    logInfo("Encoder closed");
}

// ---------------- COMMAND BUILD ----------------

std::string H264Encoder::buildCommand(const std::string& outputPath)
{
    std::ostringstream cmd;

    cmd << "ffmpeg -y "
        << "-f rawvideo -pix_fmt bgr24 "
        << "-s " << width << "x" << height << " "
        << "-r " << fps << " "
        << "-i - ";

    // -------- SAFE PLATFORM ENCODER SELECTION --------
#if defined(_WIN32)

    if (useHardware)
    {
        // Intel QuickSync fallback for Windows
        cmd << "-c:v h264_qsv ";
    }
    else
    {
        cmd << "-c:v libx264 -preset veryfast -crf 26 ";
    }

#else

    if (useHardware)
    {
        cmd << "-c:v h264_v4l2m2m ";
    }
    else
    {
        cmd << "-c:v libx264 -preset veryfast -crf 26 ";
    }

#endif

    cmd << "-pix_fmt yuv420p \"" << outputPath << "\"";

    return cmd.str();
}