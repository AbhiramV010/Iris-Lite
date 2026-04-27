#pragma once

#include <opencv2/opencv.hpp>
#include <string>
#include <cstdio>

class H264Encoder
{
public:
    H264Encoder(int width, int height, int fps, bool useHardware);
    ~H264Encoder();

    bool open(const std::string& outputPath, int crf);
    bool writeFrame(const cv::Mat& frame);
    void close();

    bool isOpen() const { return ffmpegPipe != nullptr; }

private:
    std::string buildCommand(const std::string& outputPath, int crf);

private:
    int width;
    int height;
    int fps;
    bool useHardware;

    FILE* ffmpegPipe = nullptr;
    int currentCRF = 28;
};