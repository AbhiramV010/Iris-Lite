#pragma once
#include <string>
#include <opencv2/core.hpp>

// Represents a single video frame + timestamp
struct FrameInfo
{
    cv::Mat frame;        // BGR frame
    double timestamp = 0.0;
};

// Importance map type (single-channel float)
using ImportanceMap = cv::Mat;
