#include "TemporalMemory.hpp"

void TemporalMemory::push(const cv::Mat& frame, float importance, uint64_t idx)
{
    if (shortBuf.size() >= shortMax)
        shortBuf.pop_front();

    shortBuf.push_back({ frame.clone(), importance, idx });

    if (longBuf.size() >= longMax)
        longBuf.pop_front();

    longBuf.push_back({ frame.clone(), importance, idx });
}

void TemporalMemory::decay(float rate)
{
    for (auto& item : longBuf)
        item.importance *= rate;
}

void TemporalMemory::markSceneBoundary()
{
    // Scene reset strategy:
    // keep only strongest signals in long buffer

    std::deque<MemoryItem> filtered;

    for (auto& item : longBuf)
    {
        if (item.importance > 0.3f)
            filtered.push_back(item);
    }

    longBuf = std::move(filtered);
}

void TemporalMemory::clear()
{
    shortBuf.clear();
    longBuf.clear();
}

const std::deque<MemoryItem>& TemporalMemory::shortWindow() const
{
    return shortBuf;
}

const std::deque<MemoryItem>& TemporalMemory::longWindow() const
{
    return longBuf;
}