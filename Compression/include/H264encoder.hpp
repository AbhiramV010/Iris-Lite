#pragma once

#include <opencv2/opencv.hpp>
#include <string>
#include <cstdio>

class H264encoder
{
public:
    H264encoder(int width, int height, int fps, bool useHardware);
    ~H264encoder();

    bool open(const std::string& outputPath, int crf);
    bool writeFrame(const cv::Mat& frame);
    void close();

    bool isOpen() const { return ffmpegPipe != nullptr; }

private:
    int width;
    int height;
    int fps;
    bool useHardware;

    int currentCRF = 28;

    FILE* ffmpegPipe = nullptr;
    std::string ffmpegCommand;

    std::string buildCommand(const std::string& outputPath, int crf);
};