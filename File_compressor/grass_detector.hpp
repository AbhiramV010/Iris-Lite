#pragma once
#include <opencv2/opencv.hpp>

// Returns true if grass is covered
bool isGrassOccluded(const cv::Mat& frame);

// Returns mask of foreground (non-grass)
cv::Mat getForegroundMask(const cv::Mat& frame);
