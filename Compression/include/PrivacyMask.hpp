#pragma once
#include <opencv2/core.hpp>
#include "Types.hpp"

class PrivacyMask
{
public:
    PrivacyMask(const std::vector<cv::Rect>& zones);

    // Apply black rectangles to full-resolution frame
    void apply(cv::Mat& frame);

private:
    std::vector<cv::Rect> zones;
};
