#include "H264Encoder.hpp"
#include "logging.hpp"
#include <sstream>
#include <cstring>

H264Encoder::H264Encoder(int width_, int height_, int fps_, int bitrate_, bool useHardware_)
    : width(width_), height(height_), fps(fps_), bitrate(bitrate_),
    useHardware(useHardware_), ffmpegPipe(nullptr)
{
}

H264Encoder::~H264Encoder()
{
    close();
}

bool H264Encoder::open(const std::string& outputPath)
{
    ffmpegCommand = buildCommand(outputPath);

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

    logInfo("H264Encoder opened: " + std::to_string(width) + "x" + std::to_string(height) +
        " @" + std::to_string(fps) + "fps");

    return true;
}

bool H264Encoder::writeFrame(const cv::Mat& frame)
{
    if (!ffmpegPipe || frame.empty())
        return false;

    size_t bytes = frame.total() * frame.elemSize();
    size_t written = fwrite(frame.data, 1, bytes, ffmpegPipe);

    if (written != bytes)
    {
        logError("Incomplete FFmpeg write: " + std::to_string(written) + "/" + std::to_string(bytes));
        return false;
    }

    return true;
}

void H264Encoder::close()
{
    if (ffmpegPipe)
    {
#ifdef _WIN32
        _pclose(ffmpegPipe);
#else
        pclose(ffmpegPipe);
#endif
        ffmpegPipe = nullptr;
        logInfo("H264Encoder closed");
    }
}

std::string H264Encoder::buildCommand(const std::string& outputPath)
{
    std::ostringstream cmd;

    cmd << "ffmpeg -y -f rawvideo -pix_fmt bgr24 -s " << width << "x" << height
        << " -r " << fps << " -i - ";

#if defined(__arm__) || defined(__aarch64__)
    if (useHardware)
    {
        cmd << "-c:v h264_v4l2m2m -b:v " << bitrate << "k -maxrate " << (bitrate * 1.25) << "k "
            << "-bufsize " << (bitrate * 2) << "k ";
    }
    else
    {
        cmd << "-c:v libx264 -preset veryfast -crf 26 ";
    }
#else
    cmd << "-c:v libx264 -preset veryfast -crf 26 ";
#endif

    cmd << "-pix_fmt yuv420p \"" << outputPath << "\"";

    return cmd.str();
}