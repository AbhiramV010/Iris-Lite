#pragma once
#include <opencv2/core.hpp>
#include <opencv2/objdetect.hpp>
#include "Types.hpp"

class FaceDetector
{
public:
    // Initialize cascade classifier; can be disabled if not available
    FaceDetector(int width, int height, bool enabled);

    // Detect faces and return heatmap (0-1) with Gaussian weighting around detected faces
    ImportanceMap detect(const cv::Mat& smallFrame);

private:
    int w, h;
    bool enabled;
    cv::CascadeClassifier faceCascade;

    // Generate smooth Gaussian heatmap with confidence weighting around detected faces
    ImportanceMap generateHeatmapWithConfidence(
        const std::vector<cv::Rect>& faces,
        const std::vector<double>& confidences);
};