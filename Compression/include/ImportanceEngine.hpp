#pragma once

#include <opencv2/opencv.hpp>
#include <opencv2/objdetect.hpp>

#include "ImportanceSignal.hpp"
#include "Config.hpp"

class ImportanceEngine
{
public:
    ImportanceEngine(int w, int h, const Config& cfg);

    ImportanceSignal analyze(const cv::Mat& frame,
        const cv::Mat& prev,
        uint64_t frameIndex,
        uint64_t peakFrame);

private:
    float computeTemporal(uint64_t frame, uint64_t peak);

private:
    int w, h;
    Config cfg;

    float prevMotion = 0.0f;

    // ---- FACE DETECTION ----
    cv::CascadeClassifier faceCascade;
    bool faceReady = false;

    int frameCounter = 0;
    float lastFaceScore = 0.0f;
};