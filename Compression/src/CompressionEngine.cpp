#include "CompressionEngine.hpp"
#include "logging.hpp"

#include <opencv2/opencv.hpp>
#include <chrono>
#include <thread>
#include <algorithm>

// --------------------------------------------------
// INITIALIZATION
// --------------------------------------------------

bool CompressionEngine::initialize(const Config& cfg)
{
    config = cfg;

    policy = std::make_unique<CompressionPolicy>();
    buffer = std::make_unique<SharedFrameBuffer>();
    governor = std::make_unique<SystemGovernor>();
    storage = std::make_unique<StorageManager>();

    importance = std::make_unique<ImportanceEngine>(
        config.perceptualWidth,
        config.perceptualHeight,
        config
    );

    logInfo("Initializing SharedFrameBuffer");

    if (!buffer->initialize() || !buffer->isValid())
    {
        logError("SharedFrameBuffer failed");
        return false;
    }

    encoder = std::make_unique<H264Encoder>(
        config.encodeWidth,
        config.encodeHeight,
        config.fps,
        true
    );

    storage->ensureReady();

    stop = false;
    worker = std::thread(&CompressionEngine::workerLoop, this);

    logInfo("CompressionEngine READY (Phase 4 stable core)");
    return true;
}

// --------------------------------------------------
// EVENT QUEUE
// --------------------------------------------------

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

// --------------------------------------------------
// SHUTDOWN
// --------------------------------------------------

void CompressionEngine::shutdown()
{
    stop = true;
    cv.notify_all();

    if (worker.joinable())
        worker.join();

    if (encoder)
        encoder->close();

    logInfo("CompressionEngine shutdown complete");
}

// --------------------------------------------------
// PRESSURE MODEL
// --------------------------------------------------

float CompressionEngine::computeCompressionPressure(const EventWindow& event)
{
    float sys = governor->computePressure();

    float duration =
        static_cast<float>(event.endFrame - event.startFrame);

    float durationFactor =
        std::min(1.0f, duration / (config.fps * 10.0f));

    return std::clamp(
        0.7f * sys + 0.3f * durationFactor,
        0.0f, 1.0f
    );
}

// --------------------------------------------------
// WORKER LOOP
// --------------------------------------------------

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

// --------------------------------------------------
// CORE PIPELINE
// --------------------------------------------------

// --------------------------------------------------
// CORE PIPELINE (PHASE 4: TIME-AWARE VERSION)
// --------------------------------------------------

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

    float pressure = computeCompressionPressure(event);

    logInfo("EVENT START | " + event.trigger);

    cv::Mat prevFrame;
    bool opened = false;

    float smoothedImportance = 0.5f;

    uint64_t peak =
        (event.startFrame + event.endFrame) >> 1;

    // ---------------- SCHEDULING ----------------
    float effectiveFPS =
        config.fps * (1.0f - 0.6f * pressure);

    effectiveFPS = std::clamp(effectiveFPS, 6.0f, (float)config.fps);

    auto frameInterval =
        std::chrono::microseconds(
            (int)(1e6f / effectiveFPS)
        );

    auto nextTick = std::chrono::steady_clock::now();

    for (uint64_t i = event.startFrame;
        i < event.endFrame && !stop;
        ++i)
    {
        nextTick += frameInterval;

        cv::Mat frame(
            FRAME_HEIGHT,
            FRAME_WIDTH,
            CV_8UC3,
            raw
        );

        if (frame.empty())
            continue;

        // ---------------- IMPORTANCE ----------------
        ImportanceSignal sig =
            importance->analyze(frame, prevFrame, i, peak);

        float importanceScore = sig.score;

        float adjusted =
            importanceScore * (1.0f - 0.5f * pressure);

        smoothedImportance =
            0.88f * smoothedImportance +
            0.12f * std::clamp(adjusted, 0.0f, 1.0f);

        // ---------------- GOVERNOR ----------------
        if (governor->shouldSkipFrame(smoothedImportance))
            continue;

        int crf = governor->adaptiveCRF(
            policy->computeCRF(smoothedImportance)
        );

        // ---------------- ENCODER ----------------
        if (!opened)
        {
            if (!encoder->open(path, crf))
            {
                logError("Encoder failed");
                return;
            }
            opened = true;
        }

        encoder->writeFrame(frame);
        prevFrame = frame;

        // ---------------- REAL TIMING CONTROL ----------------
        std::this_thread::sleep_until(nextTick);
    }

    if (opened)
        encoder->close();

    logInfo("EVENT END | saved=" + path);
}