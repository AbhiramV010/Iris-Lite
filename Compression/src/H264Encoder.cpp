#include "H264Encoder.hpp"
#include "logging.hpp"

#include <sstream>
#include <cstdio>
#include <cstdlib>

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

    if (useHardware)
    {
        // Raspberry Pi hardware encoder
        cmd << "ffmpeg -y "
            << "-f rawvideo -pix_fmt bgr24 "
            << "-s " << width << "x" << height << " "
            << "-r " << fps << " -i - "
            << "-an "
            << "-c:v h264_v4l2m2m "
            << "-b:v 2500k "
            << "-pix_fmt yuv420p "
            << path;
    }
    else
    {
        // Software fallback
        cmd << "ffmpeg -y "
            << "-f rawvideo -pix_fmt bgr24 "
            << "-s " << width << "x" << height << " "
            << "-r " << fps << " -i - "
            << "-an "
            << "-c:v libx264 "
            << "-preset veryfast "
            << "-crf " << crf << " "
            << "-pix_fmt yuv420p "
            << path;
    }

    return cmd.str();
}

bool H264Encoder::open(const std::string& path, int crf)
{
    currentCRF = crf;

    std::string cmd = buildCommand(path, crf);

    ffmpegPipe = popen(cmd.c_str(), "w");

    if (!ffmpegPipe)
    {
        logError("FFmpeg pipe failed");

        // FALLBACK TO SOFTWARE if hardware fails
        if (useHardware)
        {
            logWarn("Falling back to software encoder");

            useHardware = false;
            cmd = buildCommand(path, crf);
            ffmpegPipe = popen(cmd.c_str(), "w");
        }
    }

    return ffmpegPipe != nullptr;
}

bool H264Encoder::writeFrame(const cv::Mat& frame)
{
    if (!ffmpegPipe || frame.empty())
        return false;

    cv::Mat resized;
    cv::resize(frame, resized, cv::Size(width, height));

    size_t written = fwrite(resized.data, 1, resized.total() * 3, ffmpegPipe);

    return written == resized.total() * 3;
}

void H264Encoder::close()
{
    if (ffmpegPipe)
    {
        fflush(ffmpegPipe);
        pclose(ffmpegPipe);
        ffmpegPipe = nullptr;

        logInfo("Encoder closed");
    }
}
