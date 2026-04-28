#include "H264Encoder.hpp"
#include "logging.hpp"

#include <sstream>
#include <cstdio>
#include <opencv2/opencv.hpp>

H264Encoder::H264Encoder(int w, int h, int fps_, bool hw)
    : width(w), height(h), fps(fps_), useHardware(hw)
{
}

H264Encoder::~H264Encoder()
{
    close();
}

// ----------------------------------------------------
// COMMAND BUILDER
// ----------------------------------------------------
std::string H264Encoder::buildCommand(const std::string& path, int crf)
{
    std::ostringstream cmd;

    cmd << "ffmpeg -y "
        << "-f rawvideo "
        << "-pix_fmt bgr24 "
        << "-s " << width << "x" << height << " "
        << "-r " << fps << " "
        << "-i - ";

    if (useHardware)
    {
        // Pi hardware encoder (CRF NOT supported → approximate via bitrate)
        int bitrateKbps = 2000 - (crf - 18) * 80;
        if (bitrateKbps < 500) bitrateKbps = 500;
        if (bitrateKbps > 4000) bitrateKbps = 4000;

        cmd << "-c:v h264_v4l2m2m "
            << "-b:v " << bitrateKbps << "k "
            << "-pix_fmt yuv420p ";
    }
    else
    {
        cmd << "-c:v libx264 "
            << "-preset veryfast "
            << "-crf " << crf << " "
            << "-pix_fmt yuv420p ";
    }

    cmd << path;
    return cmd.str();
}

// ----------------------------------------------------
// OPEN
// ----------------------------------------------------
bool H264Encoder::open(const std::string& path, int crf)
{
    std::string cmd = buildCommand(path, crf);

    ffmpegPipe = popen(cmd.c_str(), "w");

    if (!ffmpegPipe)
    {
        logError("FFmpeg pipe failed (hardware path)");

        if (useHardware)
        {
            logWarn("Switching to software encoder fallback");
            useHardware = false;

            cmd = buildCommand(path, crf);
            ffmpegPipe = popen(cmd.c_str(), "w");
        }
    }

    if (ffmpegPipe)
    {
        logInfo("Encoder opened: " + path);
    }
    else
    {
        logError("Encoder open failed completely: " + path);
    }

    currentCRF = crf;
    return ffmpegPipe != nullptr;
}

// ----------------------------------------------------
// WRITE FRAME
// ----------------------------------------------------
bool H264Encoder::writeFrame(const cv::Mat& frame)
{
    if (!ffmpegPipe || frame.empty())
        return false;

    cv::Mat resized;

    // avoid redundant resize (cheap guard)
    if (frame.cols == width && frame.rows == height)
    {
        resized = frame;
    }
    else
    {
        cv::resize(frame, resized, cv::Size(width, height));
    }

    size_t written = fwrite(resized.data, 1, resized.total() * 3, ffmpegPipe);

    if (written == 0)
    {
        logWarn("Frame write failed (ffmpeg pipe broken)");
        return false;
    }

    return true;
}

// ----------------------------------------------------
// CLOSE
// ----------------------------------------------------
void H264Encoder::close()
{
    if (ffmpegPipe)
    {
        pclose(ffmpegPipe);
        ffmpegPipe = nullptr;

        logInfo("Encoder closed (CRF=" + std::to_string(currentCRF) + ")");
    }
}