#pragma once

#include <memory>
#include <opencv2/opencv.hpp>

#include "Config.hpp"
#include "CompressionPolicy.hpp"
#include "H264Encoder.hpp"
#include "SharedFrameBuffer.hpp"

struct EventWindow
{
    uint64_t startFrame;
    uint64_t endFrame;
    std::string trigger;
};

class CompressionEngine
{
public:
    bool initialize(const Config& cfg);
    void processEvent(const EventWindow& event);

private:
    float computeRegionImportance(const cv::Mat& frame);
    bool detectFace(const cv::Mat& frame);

private:
    Config config;

    std::unique_ptr<CompressionPolicy> policy;
    std::unique_ptr<H264Encoder> encoder;
    std::unique_ptr<SharedFrameBuffer> buffer;

    cv::CascadeClassifier faceCascade;

    float lastScore = 0.0f;
    float lastMotion = 0.0f;
};