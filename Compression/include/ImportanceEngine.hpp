#pragma once

#include <opencv2/opencv.hpp>
#include <opencv2/objdetect.hpp>

#include "Config.hpp"
#include "ImportanceSignal.hpp"

class ImportanceEngine
{
public:
    ImportanceEngine(int width, int height, const Config& cfg);
    ~ImportanceEngine();

    // Computes perceptual importance signals
    ImportanceSignal analyze(const cv::Mat& frame, const cv::Mat& prev);

private:
    int w;
    int h;
    Config cfg;

    cv::CascadeClassifier faceCascade;

    // temporal smoothing state
    float prevGlobal = 0.5f;
};