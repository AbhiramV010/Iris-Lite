#pragma once

#include <opencv2/opencv.hpp>
#include <vector>

class PrivacyMask
{
public:
    PrivacyMask(const std::vector<cv::Rect>& zones);

    void apply(cv::Mat& frame);

private:
    std::vector<cv::Rect> zones;
};