#pragma once

#include <cstdint>
#include <vector>
#include <opencv2/opencv.hpp>

struct ImportanceSignal
{
    float motion = 0.0f;
    float spatial = 0.0f;
    float temporal = 0.0f;
    float region = 0.0f;
    float face = 0.0f;

    // final fused importance
    float score = 0.0f;

    // detected faces
    std::vector<cv::Rect> faces;

    // perceptual ROI importance
    float roiImportance = 0.0f;
};