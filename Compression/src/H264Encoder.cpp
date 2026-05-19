#include "H264Encoder.hpp"
#include "logging.hpp"

#include <sstream>
#include <cstdio>
#include <opencv2/opencv.hpp>
#include <algorithm>

H264Encoder::H264Encoder(int w, int h, int fps_, bool hw)
    : width(w), height(h), fps(fps_), useHardware(hw)
{
}

H264Encoder::~H264Encoder()
{
    close();
}

std::string H264Encoder::buildCommand(
    const std::string& path,
    int crf,
    bool hw)
{
    std::ostringstream cmd;

    cmd << "ffmpeg -y -loglevel error "
        << "-f rawvideo -pix_fmt bgr24 "
        << "-s " << width << "x" << height << " "
        << "-r " << fps << " -i - ";

    if (hw)
    {
        int bitrateKbps = 2000 - (crf - 18) * 80;
        bitrateKbps = std::clamp(bitrateKbps, 500, 4000);

        cmd << "-c:v h264_v4l2m2m -b:v "
            << bitrateKbps << "k ";
    }
    else
    {
        cmd << "-c:v libx264 -preset veryfast -crf "
            << crf << " ";
    }

    cmd << "-pix_fmt yuv420p " << path;

    return cmd.str();
}

bool H264Encoder::testPipeAlive()
{
    return ffmpegPipe != nullptr;
}

bool H264Encoder::open(const std::string& path, int crf)
{
    close();

    currentPath = path;

    // TRY HARDWARE FIRST
    if (useHardware)
    {
        std::string cmd = buildCommand(path, crf, true);
        ffmpegPipe = popen(cmd.c_str(), "w");

        if (ffmpegPipe)
        {
            hwActive = true;
            logInfo("Hardware encoder started");
            logInfo(cmd);
            return true;
        }

        logWarn("Hardware encoding failed → switching to software");
    }

    // FALLBACK SOFTWARE
    std::string softCmd = buildCommand(path, crf, false);
    ffmpegPipe = popen(softCmd.c_str(), "w");

    if (!ffmpegPipe)
    {
        logError("FFmpeg failed completely");
        return false;
    }

    hwActive = false;

    logInfo("Software encoder started");
    logInfo(softCmd);

    return true;
}

bool H264Encoder::writeFrame(const cv::Mat& frame)
{
    if (!ffmpegPipe || frame.empty())
        return false;

    cv::Mat resized;

    if (frame.cols != width || frame.rows != height)
        cv::resize(frame, resized, cv::Size(width, height));
    else
        resized = frame;

    if (!resized.isContinuous())
        resized = resized.clone();

    const size_t expected = width * height * 3;

    size_t written = fwrite(resized.data, 1, expected, ffmpegPipe);

    if (written != expected)
    {
        logError("Encoder write failure → pipe likely dead");
        close();
        return false;
    }

    fflush(ffmpegPipe);
    return true;
}

void H264Encoder::setQuality(int crf)
{
    pendingCRF = std::clamp(crf, 18, 40);
    currentCRF = pendingCRF;
}

void H264Encoder::setRegionImportance(float v)
{
    regionImportance = std::clamp(v, 0.0f, 1.0f);
}

void H264Encoder::setFaceImportance(float v)
{
    faceImportance = std::clamp(v, 0.0f, 1.0f);
}

void H264Encoder::close()
{
    if (ffmpegPipe)
    {
        fflush(ffmpegPipe);
        pclose(ffmpegPipe);
        ffmpegPipe = nullptr;

        logInfo("Encoder closed (" +
            std::string(hwActive ? "HW" : "SW") +
            ")");
    }
}