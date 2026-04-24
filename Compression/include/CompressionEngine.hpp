#pragma once

#include <memory>
#include <atomic>
#include <string>
#include <vector>
#include <queue>
#include <mutex>

#include "ImportanceEngine.hpp"
#include "CompressionPolicy.hpp"
#include "H264encoder.hpp"
#include "SharedFrameBuffer.hpp"

struct EventWindow
{
    uint64_t startFrame;
    uint64_t endFrame;
    std::string trigger;
};

class CompressionEngine
{
public:
    bool initialize(const Config& cfg);
    void shutdown();

    void enqueueEvent(const EventWindow& event);
    void processQueuedEvents();

private:
    std::atomic<bool> running{ false };

    std::unique_ptr<ImportanceEngine> importance;
    std::unique_ptr<CompressionPolicy> policy;
    std::unique_ptr<H264encoder> encoder;
    std::unique_ptr<SharedFrameBuffer> sharedBuffer;

    float importanceState = 0.5f;
    float momentum = 0.5f;

    int currentCRF = -1;
    int segmentIndex = 0;

    std::queue<EventWindow> eventQueue;
    std::mutex eventMutex;

    void startNewSegment(const std::string& fileName, int crf);
};