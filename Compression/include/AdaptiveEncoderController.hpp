#pragma once
#include <opencv2/opencv.hpp>
#include <deque>

struct BufferedItem
{
    cv::Mat frame;
    float importance;
    uint64_t idx;
};

class AdaptiveEncoderController
{
public:
    void push(const cv::Mat& frame, uint64_t idx, float importance);

    const std::deque<BufferedItem>& buffer() const;

    void trimLowImportance(float threshold);

    void clear();

private:
    std::deque<BufferedItem> buf;
    size_t maxSize = 600;
};