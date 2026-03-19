#pragma once
#include <opencv2/core.hpp>
#include "Types.hpp"

class FrameProcessor
{
public:
    FrameProcessor(int width, int height);

    // Blend sharp + smooth using importance map (0–1)
    cv::Mat process(const cv::Mat& fullFrame, const ImportanceMap& importance);

private:
    int w, h;

    cv::Mat createSmooth(const cv::Mat& frame);
};
