#include "FrameWindowBuffer.hpp"
#include <opencv2/imgcodecs.hpp>

FrameWindowBuffer::FrameWindowBuffer(size_t maxFrames_)
    : maxFrames(maxFrames_)
{
}

FrameWindowBuffer::~FrameWindowBuffer()
{
    clear();
}

void FrameWindowBuffer::pushFrame(uint64_t index, const std::vector<uint8_t>& jpegData)
{
    std::lock_guard<std::mutex> lock(bufferMutex);

    frameMap[index] = jpegData;
    frameOrder.push_back(index);

    evictOldestIfNeeded();
}

cv::Mat FrameWindowBuffer::getFrame(uint64_t index)
{
    std::lock_guard<std::mutex> lock(bufferMutex);

    if (frameMap.find(index) == frameMap.end())
        return cv::Mat();

    const auto& data = frameMap[index];

    cv::Mat raw = cv::imdecode(data, cv::IMREAD_COLOR);
    return raw;
}

std::vector<cv::Mat> FrameWindowBuffer::getFrameRange(uint64_t start, uint64_t end)
{
    std::vector<cv::Mat> frames;

    for (uint64_t i = start; i <= end; i++)
    {
        frames.push_back(getFrame(i));
    }

    return frames;
}

void FrameWindowBuffer::evictOldestIfNeeded()
{
    while (frameOrder.size() > maxFrames)
    {
        uint64_t oldIndex = frameOrder.front();
        frameOrder.pop_front();

        frameMap.erase(oldIndex);
    }
}

void FrameWindowBuffer::clear()
{
    frameOrder.clear();
    frameMap.clear();
}