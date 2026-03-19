#pragma once
#include <opencv2/core.hpp>
#include <opencv2/objdetect.hpp>
#include "Types.hpp"

class FaceDetector
{
public:
    FaceDetector(int width, int height, bool enabled);

    // Returns a heatmap (float 0–1) same size as perceptual frame
    ImportanceMap detect(const cv::Mat& smallFrame);

private:
    int w, h;
    bool enabled;
    cv::CascadeClassifier faceCascade;

    ImportanceMap generateHeatmap(const std::vector<cv::Rect>& faces);
};
