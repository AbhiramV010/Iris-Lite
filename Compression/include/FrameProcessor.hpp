#pragma once
#include <opencv2/core.hpp>
#include "Types.hpp"

class FrameProcessor
{
public:
    FrameProcessor(int width, int height);

    cv::Mat process(const cv::Mat& fullFrame, const ImportanceMap& importance);

private:
    int w, h;

    cv::Mat createSmooth(const cv::Mat& frame);

    // Preallocated buffers
    cv::Mat smooth_;
    cv::Mat impResized_;
    cv::Mat imp3_;
    cv::Mat sharpF_;
    cv::Mat smoothF_;
    cv::Mat outF_;
    cv::Mat oneMinusImp_;
};
