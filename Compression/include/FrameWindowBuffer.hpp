#pragma once

#include <opencv2/opencv.hpp>
#include <unordered_map>
#include <deque>
#include <vector>
#include <cstdint>
#include <mutex>

/*
    PI-OPTIMIZED FRAME MEMORY SYSTEM

    - stores ONLY compressed JPEG frames
    - bounded memory usage (ring buffer)
    - fast lookup via index map
*/

class FrameWindowBuffer
{
public:
    explicit FrameWindowBuffer(size_t maxFrames);
    ~FrameWindowBuffer();

    // ingest frame from Python
    void pushFrame(uint64_t index, const std::vector<uint8_t>& jpegData);

    // retrieve decoded frame
    cv::Mat getFrame(uint64_t index);

    // batch retrieval for event windows
    std::vector<cv::Mat> getFrameRange(uint64_t start, uint64_t end);

    // memory management
    void clear();

private:
    size_t maxFrames;

    // ring buffer storage (bounded RAM)
    std::deque<uint64_t> frameOrder;

    // index → JPEG data
    std::unordered_map<uint64_t, std::vector<uint8_t>> frameMap;

    // thread safety (important for real-time ingestion)
    std::mutex bufferMutex;

    void evictOldestIfNeeded();
};