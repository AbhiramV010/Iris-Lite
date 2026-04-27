#pragma once

#include <memory>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <string>

#include "Config.hpp"
#include "CompressionPolicy.hpp"
#include "H264Encoder.hpp"
#include "SharedFrameBuffer.hpp"
#include "SystemGovernor.hpp"

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
    void enqueueEvent(const EventWindow& event);
    void shutdown();

private:
    void workerLoop();
    void processEvent(const EventWindow& event);
    float getPressureThrottle();

private:
    Config config;

    std::atomic<bool> stop{ false };
    std::thread worker;

    std::queue<EventWindow> queue;
    std::mutex mtx;
    std::condition_variable cv;

    std::unique_ptr<CompressionPolicy> policy;
    std::unique_ptr<H264Encoder> encoder;
    std::unique_ptr<SharedFrameBuffer> buffer;
    std::unique_ptr<SystemGovernor> governor;

    float lastScore = 0.5f;
    float lastMotion = 0.0f;
};