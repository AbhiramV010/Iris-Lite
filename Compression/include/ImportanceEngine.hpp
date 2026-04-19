#pragma once

#include <opencv2/opencv.hpp>
#include "ImportanceResult.hpp"
#include "Config.hpp"
#include <opencv2/objdetect.hpp>
#include "ImportanceSignal.hpp"

class ImportanceEngine
{
public:
    ImportanceEngine(int width, int height, const Config& cfg);
    ~ImportanceEngine();



    ImportanceSignal analyze(const cv::Mat& frame, const cv::Mat& prev);

private:
    int w;
    int h;
    const Config& cfg;

    cv::CascadeClassifier faceCascade;
};