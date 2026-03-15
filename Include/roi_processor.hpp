#pragma once
#include <opencv2/opencv.hpp>

// This class handles region-adaptive processing.
// It keeps important/moving areas sharp and reduces detail in the background.
// The goal is to save storage while keeping the video looking normal.

class ROIProcessor {
public:
    cv::Mat processFrame(const cv::Mat& frame);
};
