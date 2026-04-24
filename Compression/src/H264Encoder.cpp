#include "H264Encoder.hpp"
#include "logging.hpp"
#include <sstream>

H264Encoder::H264Encoder(int w, int h, int fps, bool hw)
    : width(w), height(h), fps(fps), useHardware(hw) {
}

H264Encoder::~H264Encoder()
{
    close();
}

bool H264Encoder::open(const std::string& path, int crf)
{
    currentCRF = crf;
    ffmpegCommand = buildCommand(path, crf);

    ffmpegPipe = popen(ffmpegCommand.c_str(), "w");

    if (!ffmpegPipe)
        return false;

    return true;
}

bool H264Encoder::writeFrame(const cv::Mat& frame)
{
    if (!ffmpegPipe || frame.empty())
        return false;

    cv::Mat bgr;
    cv::resize(frame, bgr, cv::Size(width, height));

    if (bgr.channels() != 3)
        cv::cvtColor(bgr, bgr, cv::COLOR_GRAY2BGR);

    size_t bytes = bgr.total() * bgr.elemSize();

    if (fwrite(bgr.data, 1, bytes, ffmpegPipe) != bytes)
    {
        logError("FFmpeg write failed");
        close();
        return false;
    }

    return true;
}

void H264Encoder::close()
{
    if (ffmpegPipe)
        pclose(ffmpegPipe);

    ffmpegPipe = nullptr;
}
