#pragma once
#include <opencv2/core.hpp>
#include "Types.hpp"

class FrameProcessor
{
public:
    // Initialize processor with target output resolution
    FrameProcessor(int width, int height);

    // Apply perceptual blending: sharp in high-importance regions, smooth in low-importance regions
    cv::Mat process(const cv::Mat& fullFrame, const ImportanceMap& importance);

private:
    int w, h;

    // Create smoothed version of frame using bilateral filtering (edge-preserving blur)
    cv::Mat createSmoothBilateral(const cv::Mat& frame);

    // Preallocated buffers for efficiency (avoid allocation per frame)
    cv::Mat smooth_;
    cv::Mat impResized_;
    cv::Mat imp3_;
    cv::Mat sharpF_;
    cv::Mat smoothF_;
    cv::Mat outF_;
    cv::Mat oneMinusImp_;
};