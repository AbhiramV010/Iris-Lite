#include "CompressionEngine.hpp"
#include "SharedMemoryConfig.hpp"
#include "logging.hpp"

#include <opencv2/opencv.hpp>
#include <chrono>
#include <thread>
#include <cmath>

// ---------------- SYSTEM LOAD ----------------
float CompressionEngine::getSystemLoad()
{
    return std::clamp(importanceState, 0.0f, 1.5f);
}

// ---------------- INIT ----------------
bool CompressionEngine::initialize(const Config& cfg)
{
    importance = std::make_unique<ImportanceEngine>(
        cfg.perceptualWidth,
        cfg.perceptualHeight,
        cfg
    );

    policy = std::make_unique<CompressionPolicy>();

    encoder = std::make_unique<H264Encoder>(
        cfg.encodeWidth,
        cfg.encodeHeight,
        cfg.fps,
        true
    );

    sharedBuffer = std::make_unique<SharedFrameBuffer>();

    if (!sharedBuffer->initialize())
    {
        logError("SharedFrameBuffer init failed");
        return false;
    }

    running = true;
    stopWorker = false;

    worker = std::thread(&CompressionEngine::workerLoop, this);

    return true;
}

// ---------------- ENQUEUE ----------------
void CompressionEngine::enqueueEvent(const EventWindow& event)
{
    {
        std::lock_guard<std::mutex> lock(eventMutex);
        eventQueue.push(event);
    }
    cv.notify_one();
}

// ---------------- WORKER LOOP ----------------
void CompressionEngine::workerLoop()
{
    while (!stopWorker)
    {
        EventWindow event;

        {
            std::unique_lock<std::mutex> lock(eventMutex);

            cv.wait(lock, [&] {
                return stopWorker || !eventQueue.empty();
                });

            if (stopWorker)
                return;

            event = eventQueue.front();
            eventQueue.pop();
        }

        processEvent(event);
    }
}

// ---------------- PROCESS EVENT ----------------
void CompressionEngine::processEvent(const EventWindow& event)
{
    std::string path = "/media/pi/clipDrive/iris_lite--" + std::to_string(event.startFrame) + ".mp4";

    int adaptiveCRF = policy->computeCRF(
        importanceState,
        momentum,
        currentCRF
    );

    startNewSegment(path, adaptiveCRF);

    cv::Mat prev;

    for (uint64_t i = event.startFrame; i < event.endFrame; i++)
    {
        std::vector<uint8_t> jpeg;

        if (!sharedBuffer->getFrame(i, jpeg))
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            continue;
        }

        cv::Mat frame = cv::imdecode(jpeg, cv::IMREAD_COLOR);

        if (frame.empty())
            continue;

        // ✅ FIXED: correct function signature
        ImportanceSignal signal = importance->analyze(frame, prev);

        // smoothing
        importanceState = 0.9f * importanceState + 0.1f * signal.global;
        momentum = 0.85f * momentum + 0.15f * importanceState;

        bool keep = policy->shouldKeepFrame(
            importanceState,
            momentum,
            i
        );

        if (keep && encoder && encoder->isOpen())
            encoder->writeFrame(frame);

        prev = frame;
    }

    if (encoder && encoder->isOpen())
        encoder->close();
}

// ---------------- START SEGMENT ----------------
void CompressionEngine::startNewSegment(const std::string& fileName, int crf)
{
    if (encoder && encoder->isOpen())
        encoder->close();

    if (!encoder->open(fileName, crf))
    {
        logError("Failed to open encoder: " + fileName);
        running = false;
    }
}

// ---------------- SHUTDOWN ----------------
void CompressionEngine::shutdown()
{
    stopWorker = true;
    cv.notify_all();

    if (worker.joinable())
        worker.join();

    if (encoder)
        encoder->close();
}