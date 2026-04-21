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

bool H264Encoder::writeFrame(const cv::Mat& frame)
{
    if (!ffmpegPipe || frame.empty())
        return false;

    size_t bytes = frame.total() * frame.elemSize();
    size_t writtenBytes = fwrite(frame.data, 1, bytes, ffmpegPipe);

    if (writtenBytes != bytes)
    {
        logError("FFmpeg write failed");

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

std::string H264Encoder::buildCommand(const std::string& outputPath)
{
    std::ostringstream cmd;

    cmd << "ffmpeg -y "
        << "-f rawvideo -pix_fmt bgr24 "
        << "-s " << width << "x" << height << " "
        << "-r " << fps << " "
        << "-i - ";

#if defined(_WIN32)

    cmd << "-c:v libx264 -preset veryfast -crf " << bitrate;

#else

    if (useHardware)
        cmd << "-c:v h264_v4l2m2m ";
    else
        cmd << "-c:v libx264 -preset veryfast -crf " << bitrate;

#endif

    cmd << " -pix_fmt yuv420p \"" << outputPath << "\"";

    return cmd.str();
}

void H264Encoder::setCRF(int newCRF)
{
    bitrate = newCRF;
}