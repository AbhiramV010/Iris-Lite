#pragma once
#include <opencv2/opencv.hpp>
#include <deque>

struct MemoryItem
{
    cv::Mat frame;
    float importance;
    uint64_t idx;
};

class TemporalMemory
{
public:
    void push(const cv::Mat& frame, float importance, uint64_t idx);

    void clear();

    void decay(float rate);

    const std::deque<MemoryItem>& shortWindow() const;
    const std::deque<MemoryItem>& longWindow() const;

    void markSceneBoundary();

private:
    std::deque<MemoryItem> shortBuf;  // ~2s window
    std::deque<MemoryItem> longBuf;    // ~20–30s window

    size_t shortMax = 60;
    size_t longMax = 600;
};