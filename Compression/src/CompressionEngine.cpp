#include "CompressionEngine.hpp"
#include "SharedMemoryConfig.hpp"

#include <opencv2/opencv.hpp>
#include <chrono>
#include <sys/sysinfo.h>
#include <cmath>
#include <algorithm>

// ---------------- INIT ----------------

bool CompressionEngine::initialize(const Config& cfg)
{
    importance = std::make_unique<ImportanceEngine>(
        cfg.perceptualWidth,
        cfg.perceptualHeight,
        cfg
    );

    policy = std::make_unique<CompressionPolicy>();

    encoder = std::make_unique<H264encoder>(
        cfg.encodeWidth,
        cfg.encodeHeight,
        cfg.fps,
        true
    );

    sharedBuffer = std::make_unique<SharedFrameBuffer>();
    if (!sharedBuffer->initialize())
        return false;

    importanceState = 0.5f;
    momentum = 0.5f;
    currentCRF = cfg.crf;
    segmentIndex = 0;

    running = true;
    return true;
}

// ---------------- SEGMENT ----------------

void CompressionEngine::startNewSegment(const std::string& fileName, int crf)
{
    if (encoder->isOpen())
        encoder->close();

    if (!encoder->open(fileName, crf))
    {
        running = false;
        return;
    }

    currentCRF = crf;
}

// ---------------- EVENT QUEUE ----------------

void CompressionEngine::enqueueEvent(const EventWindow& event)
{
    std::lock_guard<std::mutex> lock(eventMutex);
    eventQueue.push(event);
}

void CompressionEngine::processQueuedEvents()
{
    while (true)
    {
        EventWindow event;

        {
            std::lock_guard<std::mutex> lock(eventMutex);

            if (eventQueue.empty())
                break;

            event = eventQueue.front();
            eventQueue.pop();
        }

        // run clip
        auto now = std::chrono::system_clock::now();
        std::string ts = std::format("{:%Y-%m-%d_%H-%M-%S}", now);

        std::string path =
            "/clipDrive/clips/iris_lite--" + ts + ".mp4";

        startNewSegment(path, currentCRF);
    }
}

// ---------------- LOAD GUARD ----------------

static float getSystemLoad()
{
    struct sysinfo info;
    sysinfo(&info);
    return (float)info.loads[0] / 65536.0f;
}

// ---------------- SHUTDOWN ----------------

void CompressionEngine::shutdown()
{
    running = false;

    if (encoder)
        encoder->close();
}