#pragma once

#include <memory>
#include <atomic>
#include <string>
#include <queue>
#include <mutex>
#include <thread>
#include <condition_variable>

#include "ImportanceEngine.hpp"
#include "CompressionPolicy.hpp"
#include "H264Encoder.hpp"
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

private:
    void workerLoop();
    void processEvent(const EventWindow& event);
    void startNewSegment(const std::string& fileName, int crf);

private:
    std::atomic<bool> running{ false };

    std::unique_ptr<ImportanceEngine> importance;
    std::unique_ptr<CompressionPolicy> policy;
    std::unique_ptr<H264Encoder> Encoder;
    std::unique_ptr<SharedFrameBuffer> sharedBuffer;

    float importanceState = 0.5f;
    float momentum = 0.5f;

    int currentCRF = 28;
    int segmentIndex = 0;

    std::queue<EventWindow> eventQueue;
    std::mutex eventMutex;
    std::condition_variable cv;

    std::thread worker;
    bool stopWorker = false;
};