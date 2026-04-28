#include "H264Encoder.hpp"
#include "logging.hpp"

#include <sstream>
#include <cstdio>
#include <opencv2/opencv.hpp>
#include <algorithm>

// ----------------------------------------------------
// CONSTRUCTOR / DESTRUCTOR
// ----------------------------------------------------

H264Encoder::H264Encoder(int w, int h, int fps_, bool hw)
    : width(w), height(h), fps(fps_), useHardware(hw)
{
}

H264Encoder::~H264Encoder()
{
    close();
}

// ----------------------------------------------------
// BUILD FFMPEG COMMAND
// ----------------------------------------------------

std::string H264Encoder::buildCommand(const std::string& path, int crf)
{
    std::ostringstream cmd;

    cmd << "ffmpeg -y "
        << "-loglevel error "
        << "-f rawvideo "
        << "-pix_fmt bgr24 "
        << "-s " << width << "x" << height << " "
        << "-r " << fps << " "
        << "-i - ";

    if (useHardware)
    {
        int bitrateKbps = 2000 - (crf - 18) * 80;
        bitrateKbps = std::clamp(bitrateKbps, 500, 4000);

        cmd << "-c:v h264_v4l2m2m "
            << "-b:v " << bitrateKbps << "k ";
    }
    else
    {
        cmd << "-c:v libx264 "
            << "-preset veryfast "
            << "-crf " << crf << " ";
    }

    cmd << "-pix_fmt yuv420p "
        << path;

    return cmd.str();
}

// ----------------------------------------------------
// OPEN ENCODER
// ----------------------------------------------------

bool H264Encoder::open(const std::string& path, int crf)
{
    close(); // ensure clean state

    std::string cmd = buildCommand(path, crf);

    ffmpegPipe = popen(cmd.c_str(), "w");

    if (!ffmpegPipe)
    {
        logError("FFmpeg failed to start");
        return false;
    }

    currentCRF = crf;

    logInfo("Encoder started: " + path);
    logInfo("FFmpeg CMD: " + cmd);

    return true;
}

// ----------------------------------------------------
// WRITE FRAME (CRITICAL FIXED VERSION)
// ----------------------------------------------------

bool H264Encoder::writeFrame(const cv::Mat& frame)
{
    if (!ffmpegPipe || frame.empty())
        return false;

    cv::Mat resized;

    // ensure correct size
    if (frame.cols != width || frame.rows != height)
        cv::resize(frame, resized, cv::Size(width, height));
    else
        resized = frame;

    // ensure memory is contiguous (CRITICAL for ffmpeg)
    if (!resized.isContinuous())
        resized = resized.clone();

    const size_t expectedBytes = width * height * 3;

    size_t written = fwrite(resized.data, 1, expectedBytes, ffmpegPipe);

    if (written != expectedBytes)
    {
        logError("FFmpeg write mismatch: " +
            std::to_string(written) + "/" +
            std::to_string(expectedBytes));
        return false;
    }

    // CRITICAL: force pipe flush (prevents silent empty output)
    fflush(ffmpegPipe);

    return true;
}

// ----------------------------------------------------
// CLOSE ENCODER
// ----------------------------------------------------

void H264Encoder::close()
{
    if (ffmpegPipe)
    {
        fflush(ffmpegPipe);
        pclose(ffmpegPipe);
        ffmpegPipe = nullptr;

        logInfo("Encoder closed (CRF=" + std::to_string(currentCRF) + ")");
    }
}