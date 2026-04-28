#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <memory>
#include <string>

#include "Config.hpp"
#include "CompressionPolicy.hpp"
#include "H264Encoder.hpp"
#include "SharedFrameBuffer.hpp"
#include "SystemGovernor.hpp"
#include "EventCluster.hpp"

struct EventWindow
{
    uint64_t startFrame;
    uint64_t endFrame;
    std::string trigger;
};

struct FrameImportance
{
    float score;
    bool isPeak;
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

    float computeTemporalWeight(uint64_t frame, uint64_t peak);

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

    EventCluster cluster;

    float lastImportance = 0.5f;
};