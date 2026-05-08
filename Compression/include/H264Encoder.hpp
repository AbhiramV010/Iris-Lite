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

    // runtime adaptive quality
    void setQuality(int crf);

    // perceptual weighting
    void setRegionImportance(float value);
    void setFaceImportance(float value);

    void close();

    bool isOpen() const
    {
        return ffmpegPipe != nullptr;
    }

private:
    std::string buildCommand(const std::string& outputPath, int crf);

private:
    int width;
    int height;
    int fps;
    bool useHardware;

    FILE* ffmpegPipe = nullptr;

    // adaptive runtime state
    int currentCRF = 28;
    int pendingCRF = 28;

    // perceptual runtime state
    float regionImportance = 0.0f;
    float faceImportance = 0.0f;
};