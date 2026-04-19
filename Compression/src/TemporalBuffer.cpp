#include "TemporalBuffer.hpp"

void TemporalBuffer::push(const cv::Mat& frame, float importance, uint64_t idx)
{
    if (buffer.size() >= maxSize)
        buffer.pop_front();

    buffer.push_back({ frame.clone(), importance, idx });
}

const std::deque<TimedFrame>& TemporalBuffer::data() const
{
    return buffer;
}

void TemporalBuffer::clear()
{
    buffer.clear();
}