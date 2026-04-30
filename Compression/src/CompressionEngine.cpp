#include "CompressionEngine.hpp"
#include "logging.hpp"

#include <opencv2/opencv.hpp>
#include <chrono>
#include <thread>
#include <algorithm>

// ---------------- INIT ----------------

bool CompressionEngine::initialize(const Config& cfg)
{
    config = cfg;

    buffer = std::make_unique<SharedFrameBuffer>();
    governor = std::make_unique<SystemGovernor>();
    storage = std::make_unique<StorageManager>();

    encoder = std::make_unique<H264Encoder>(
        config.encodeWidth,
        config.encodeHeight,
        config.fps,
        true
    );

    orchestrator = std::make_unique<CompressionOrchestrator>(
        config,
        nullptr,   // wire externally if needed
        governor.get(),
        nullptr,
        nullptr
    );

    if (!buffer->initialize() || !buffer->isValid())
    {
        logError("SharedFrameBuffer failed");
        return false;
    }

    storage->ensureReady();

    stop = false;
    worker = std::thread(&CompressionEngine::workerLoop, this);

    logInfo("CompressionEngine READY (Fidelity Mode)");
    return true;
}

// ---------------- QUEUE ----------------

void CompressionEngine::enqueueEvent(const EventWindow& event)
{
    cluster.add(event);

    if (!cluster.shouldFlush())
        return;

    auto events = cluster.flush();

    std::lock_guard<std::mutex> lock(mtx);
    for (const auto& e : events)
        queue.push(e);

    cv.notify_one();
}

// ---------------- SHUTDOWN ----------------

void CompressionEngine::shutdown()
{
    stop = true;
    cv.notify_all();

    if (worker.joinable())
        worker.join();

    if (encoder)
        encoder->close();

    logInfo("Shutdown complete");
}

// ---------------- WORKER ----------------

void CompressionEngine::workerLoop()
{
    while (!stop)
    {
        EventWindow event;

        {
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait(lock, [&] {
                return stop || !queue.empty();
                });

            if (stop && queue.empty())
                break;

            event = queue.front();
            queue.pop();
        }

        processEvent(event);
    }
}

// ---------------- CORE PIPELINE ----------------

void CompressionEngine::processEvent(const EventWindow& event)
{
    uint8_t* raw = buffer->getFramePtr();
    if (!raw)
        return;

    std::string path =
        storage->buildPath(event.startFrame, event.endFrame, event.trigger);

    logInfo("EVENT START | " + event.trigger);

    cv::Mat prevFrame;
    bool opened = false;

    auto nextTick = std::chrono::steady_clock::now();

    for (uint64_t i = event.startFrame;
        i < event.endFrame && !stop;
        ++i)
    {
        nextTick += std::chrono::microseconds(1000000 / config.fps);

        cv::Mat frame(
            FRAME_HEIGHT,
            FRAME_WIDTH,
            CV_8UC3,
            raw
        );

        if (frame.empty())
            continue;

        // ---------------- SINGLE DECISION SOURCE ----------------
        auto decision = orchestrator->compute(
            frame,
            prevFrame,
            i,
            (event.startFrame + event.endFrame) >> 1,
            event.trigger
        );

        if (decision.dropFrame)
        {
            prevFrame = frame;
            continue;
        }

        if (!opened)
        {
            if (!encoder->open(path, decision.crf))
            {
                logError("Encoder failed");
                return;
            }
            opened = true;
        }

        encoder->writeFrame(frame);
        prevFrame = frame;

        // ---------------- CPU ONLY AFFECTS TIMING ----------------
        float pressure = governor->computePressure();

        std::this_thread::sleep_until(
            nextTick +
            std::chrono::microseconds(
                (int)(pressure * 5000) // pacing only
            )
        );
    }

    if (opened)
        encoder->close();

    logInfo("EVENT END | saved=" + path);
}