#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <memory>

#include "EventTypes.hpp"
#include "Config.hpp"
#include "CompressionOrchestrator.hpp"
#include "H264Encoder.hpp"
#include "SharedFrameBuffer.hpp"
#include "SystemGovernor.hpp"
#include "EventCluster.hpp"
#include "StorageManager.hpp"

class CompressionEngine
{
public:
    bool initialize(const Config& cfg);
    void enqueueEvent(const EventWindow& event);
    void shutdown();

private:
    void workerLoop();
    void processEvent(const EventWindow& event);

private:
    Config config;

    std::atomic<bool> stop{ false };
    std::thread worker;

    std::queue<EventWindow> queue;
    std::mutex mtx;
    std::condition_variable cv;

    // CORE SYSTEMS
    std::unique_ptr<CompressionOrchestrator> orchestrator;
    std::unique_ptr<H264Encoder> encoder;
    std::unique_ptr<SharedFrameBuffer> buffer;
    std::unique_ptr<SystemGovernor> governor;
    std::unique_ptr<StorageManager> storage;

    EventCluster cluster;
};