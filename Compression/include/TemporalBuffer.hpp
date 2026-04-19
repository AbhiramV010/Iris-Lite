#pragma once
#include <opencv2/opencv.hpp>
#include <deque>

struct TimedFrame
{
    cv::Mat frame;
    float importance;
    uint64_t idx;
};

class TemporalBuffer
{
public:
    void push(const cv::Mat& frame, float importance, uint64_t idx);

    const std::deque<TimedFrame>& data() const;

    void clear();

private:
    std::deque<TimedFrame> buffer;
    size_t maxSize = 60; // ~2 seconds at 30fps
};