#include "EventFrameReconstructor.hpp"

EventFrameReconstructor::EventFrameReconstructor(SharedFrameBuffer* buf)
    : buffer(buf) {
}

bool EventFrameReconstructor::getOrderedEventFrames(
    uint64_t start,
    uint64_t end,
    std::vector<cv::Mat>& out)
{
    if (!buffer)
        return false;

    for (uint64_t i = start; i <= end; ++i)
    {
        std::vector<uint8_t> jpeg;
        uint32_t size = 0;

        if (!buffer->getFrame(i, jpeg, size))
            continue;

        cv::Mat frame = cv::imdecode(jpeg, cv::IMREAD_COLOR);

        if (!frame.empty())
            out.push_back(frame);
    }

    return !out.empty();
}