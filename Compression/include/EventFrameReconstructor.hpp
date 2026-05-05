#pragma once

#include <vector>
#include <cstdint>
#include <opencv2/opencv.hpp>
#include "SharedFrameBuffer.hpp"

class EventFrameReconstructor
{
public:
    explicit EventFrameReconstructor(SharedFrameBuffer* buf);

    bool getOrderedEventFrames(uint64_t start,
        uint64_t end,
        std::vector<cv::Mat>& out);

private:
    SharedFrameBuffer* buffer;
};