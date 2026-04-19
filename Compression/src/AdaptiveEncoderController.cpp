#include "AdaptiveEncoderController.hpp"

void AdaptiveEncoderController::push(const cv::Mat& frame, uint64_t idx, float importance)
{
    if (buf.size() >= maxSize)
        buf.pop_front();

    buf.push_back({ frame.clone(), importance, idx });
}

const std::deque<BufferedItem>& AdaptiveEncoderController::buffer() const
{
    return buf;
}

void AdaptiveEncoderController::trimLowImportance(float threshold)
{
    std::deque<BufferedItem> filtered;

    for (auto& item : buf)
    {
        if (item.importance >= threshold)
            filtered.push_back(item);
    }

    buf = std::move(filtered);
}

void AdaptiveEncoderController::clear()
{
    buf.clear();
}