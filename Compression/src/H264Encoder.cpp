#include "H264Encoder.hpp"
#include "logging.hpp"

#include <sstream>
#include <cstdio>

H264Encoder::H264Encoder(int w, int h, int fps_, bool hw)
    : width(w), height(h), fps(fps_), useHardware(hw)
{
}

H264Encoder::~H264Encoder()
{
    close();
}

std::string H264Encoder::buildCommand(const std::string& path, int crf)
{
    std::ostringstream cmd;

    cmd << "ffmpeg -y -f rawvideo -pix_fmt bgr24 "
        << "-s " << width << "x" << height << " "
        << "-r " << fps << " -i - "
        << "-c:v libx264 -preset ultrafast -crf " << crf
        << " -pix_fmt yuv420p " << path;

    return cmd.str();
}

bool H264Encoder::open(const std::string& path, int crf)
{
    std::string cmd = buildCommand(path, crf);

    ffmpegPipe = popen(cmd.c_str(), "w");

    if (!ffmpegPipe)
        logError("FFmpeg pipe failed");

    return ffmpegPipe != nullptr;
}

bool H264Encoder::writeFrame(const cv::Mat& frame)
{
    if (!ffmpegPipe || frame.empty())
        return false;

    cv::Mat resized;
    cv::resize(frame, resized, cv::Size(width, height));

    fwrite(resized.data, 1, resized.total() * 3, ffmpegPipe);
    return true;
}

void H264Encoder::close()
{
    if (ffmpegPipe)
    {
        pclose(ffmpegPipe);
        ffmpegPipe = nullptr;
        logInfo("Encoder closed");
    }
}