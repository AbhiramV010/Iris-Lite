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

    void setQuality(int crf);
    void setRegionImportance(float value);
    void setFaceImportance(float value);

    void close();

    bool isOpen() const { return ffmpegPipe != nullptr; }

private:
    std::string buildCommand(const std::string& outputPath, int crf, bool hw);

    bool testPipeAlive();

private:
    int width;
    int height;
    int fps;
    bool useHardware;

    FILE* ffmpegPipe = nullptr;
    std::string currentPath;

    bool hwActive = false;

    int currentCRF = 28;
    int pendingCRF = 28;

    float regionImportance = 0.0f;
    float faceImportance = 0.0f;
};