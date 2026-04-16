#pragma once

#include <opencv2/core.hpp>
#include <string>
#include <cstdio>

class H264Encoder
{
public:
    H264Encoder(int width, int height, int fps, int bitrate, bool useHardware);
    ~H264Encoder();

    bool open(const std::string& outputPath);

    bool writeFrame(const cv::Mat& frame);

    void close();

    bool isOpen() const { return ffmpegPipe != nullptr; }

private:
    int width, height, fps, bitrate;
    bool useHardware;
    FILE* ffmpegPipe;
    std::string ffmpegCommand;

    std::string buildCommand(const std::string& outputPath);
};