#include "H264encoder.hpp"
#include "logging.hpp"
#include <sstream>

H264encoder::H264encoder(int w, int h, int fps, bool hw)
    : width(w), height(h), fps(fps), useHardware(hw) {
}

H264encoder::~H264encoder()
{
    close();
}

bool H264encoder::open(const std::string& path, int crf)
{
    currentCRF = crf;
    ffmpegCommand = buildCommand(path, crf);

    ffmpegPipe = popen(ffmpegCommand.c_str(), "w");

    if (!ffmpegPipe)
        return false;

    return true;
}

bool H264encoder::writeFrame(const cv::Mat& frame)
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

void H264encoder::close()
{
    if (ffmpegPipe)
        pclose(ffmpegPipe);

    ffmpegPipe = nullptr;
}