#include "H264Encoder.hpp"
#include "logging.hpp"

#include <sstream>

H264Encoder::H264Encoder(int width_, int height_, int fps_, bool useHardware_)
    : width(width_),
    height(height_),
    fps(fps_),
    useHardware(useHardware_)
{
}

H264Encoder::~H264Encoder()
{
    close();
}

// ---------------- OPEN ----------------

bool H264Encoder::open(const std::string& outputPath, int crf)
{
    currentCRF = crf;

    ffmpegCommand = buildCommand(outputPath, crf);

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

    return true;
}

// ---------------- WRITE FRAME ----------------

bool H264Encoder::writeFrame(const cv::Mat& frame)
{
    if (!ffmpegPipe || frame.empty())
        return false;

    // enforce format safety (VERY IMPORTANT for Pi stability)
    cv::Mat bgrFrame;

    if (frame.channels() == 3)
        bgrFrame = frame;
    else
        cv::cvtColor(frame, bgrFrame, cv::COLOR_GRAY2BGR);

    size_t bytes = bgrFrame.total() * bgrFrame.elemSize();
    size_t written = fwrite(bgrFrame.data, 1, bytes, ffmpegPipe);

    if (written != bytes)
    {
        logError("FFmpeg write failed");
        close();
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

// ---------------- FFmpeg COMMAND ----------------

std::string H264Encoder::buildCommand(const std::string& outputPath, int crf)
{
    std::ostringstream cmd;

    cmd << "ffmpeg -y "
        << "-f rawvideo -pix_fmt bgr24 "
        << "-s " << width << "x" << height << " "
        << "-r " << fps << " "
        << "-i - ";

#if defined(_WIN32)
    cmd << "-c:v libx264 -preset ultrafast -crf " << crf;
#else
    if (useHardware)
        cmd << "-c:v h264_v4l2m2m ";
    else
        cmd << "-c:v libx264 -preset ultrafast -crf " << crf;
#endif

    cmd << " -pix_fmt yuv420p \"" << outputPath << "\"";

    return cmd.str();
}