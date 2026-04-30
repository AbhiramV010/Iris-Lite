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

   
    importance = std::make_unique<ImportanceEngine>(
        config.perceptualWidth,
        config.perceptualHeight,
        config
    );

    policy = std::make_unique<CompressionPolicy>();

    orchestrator = std::make_unique<CompressionOrchestrator>(
        config,
        importance.get(),
        governor.get(),
        policy.get(),
        nullptr   // memory optional
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

    if (encoder && encoder->isOpen())
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
    {
        logError("Null frame pointer");
        return;
    }

    std::string path =
        storage->buildPath(event.startFrame, event.endFrame, event.trigger);

    logInfo("EVENT START | " + event.trigger);

    cv::Mat prevFrame;
    bool opened = false;

    auto baseInterval =
        std::chrono::microseconds(1000000 / config.fps);

    auto nextTick = std::chrono::steady_clock::now();

    const uint64_t peak =
        (event.startFrame + event.endFrame) >> 1;

    for (uint64_t i = event.startFrame;
        i < event.endFrame && !stop;
        ++i)
    {
        nextTick += baseInterval;

       
        cv::Mat frame(
            FRAME_HEIGHT,
            FRAME_WIDTH,
            CV_8UC3,
            raw
        );

        if (frame.empty())
            continue;

        cv::Mat safeFrame = frame.clone();

        // ---------------- DECISION ----------------
        auto decision = orchestrator->compute(
            safeFrame,
            prevFrame,
            i,
            peak,
            event.trigger
        );

        // ---------------- DROP ----------------
        if (decision.dropFrame)
        {
            prevFrame = safeFrame;
            continue;
        }

        // ---------------- ENCODER ----------------
        if (!opened)
        {
            if (!encoder->open(path, decision.crf))
            {
                logError("Encoder failed");
                return;
            }
            opened = true;
        }

        encoder->writeFrame(safeFrame);
        prevFrame = safeFrame;

        // ---------------- CPU AFFECTS TIMING ONLY ----------------
        float pressure = governor->computePressure();

        auto delay =
            std::chrono::microseconds(
                static_cast<int>(pressure * 4000) // tighter control
            );

        std::this_thread::sleep_until(nextTick + delay);
    }

    if (opened && encoder->isOpen())
        encoder->close();

    logInfo("EVENT END | saved=" + path);
}