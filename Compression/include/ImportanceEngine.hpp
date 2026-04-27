#pragma once

#include <opencv2/opencv.hpp>
#include "ImportanceSignal.hpp"
#include "Config.hpp"

class ImportanceEngine
{
public:
    ImportanceEngine(int w, int h, const Config& cfg);
    ImportanceSignal analyze(const cv::Mat& frame,
        const cv::Mat& prev);

private:
    int w, h;
    Config cfg;

    float prevMotion = 0.0f;
};