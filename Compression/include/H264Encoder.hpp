#pragma once

#include <opencv2/opencv.hpp>
#include <string>
#include <cstdio>
#include <vector>

class H264Encoder
{
public:
    H264Encoder(int width, int height, int fps, bool useHardware);
    ~H264Encoder();

    bool open(const std::string& outputPath, int crf);
    bool writeFrame(const cv::Mat& frame);
    bool writeAudio(const std::vector<uint8_t>& pcm);
    void close();

    bool isOpen() const { return ffmpegPipe != nullptr; }

private:
    int width;
    int height;
    int fps;
    bool useHardware;

    int currentCRF = 28;

    FILE* ffmpegPipe = nullptr;
    FILE* audioPipe = nullptr;
    std::string ffmpegCommand;

    std::string buildCommand(const std::string& outputPath, int crf);
};
