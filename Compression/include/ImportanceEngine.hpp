#pragma once

#include <opencv2/opencv.hpp>
#include "ImportanceSignal.hpp"
#include "Config.hpp"

class ImportanceEngine
{
public:
    ImportanceEngine(int width, int height, const Config& cfg);
    ~ImportanceEngine();

    ImportanceSignal analyze(const cv::Mat& frame, const cv::Mat& prev);

private:
    int w;
    int h;

    Config cfg;

    cv::CascadeClassifier faceCascade;

    float prevGlobal = 0.5f;
};