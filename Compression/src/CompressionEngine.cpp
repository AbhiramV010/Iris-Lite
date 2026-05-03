#include "CompressionEngine.hpp"
#include "logging.hpp"
#include "SharedMemoryConfig.hpp"

#include <opencv2/opencv.hpp>
#include <chrono>
#include <thread>

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

    logInfo("CompressionEngine READY (STABLE SINGLE-FRAME MODE)");
    return true;
}

// ---------------- EVENT QUEUE ----------------

void CompressionEngine::enqueueEvent(const EventWindow& event)
{
    logInfo("EVENT IN | " + event.trigger);

    cluster.add(event);
    auto events = cluster.flush();

    if (events.empty())
        events.push_back(event);

    std::lock_guard<std::mutex> lock(mtx);

    for (const auto& e : events)
        queue.push(e);

    cv.notify_one();
}

// ---------------- WORKER ----------------

void CompressionEngine::workerLoop()
{
    logInfo("Worker loop started");

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
    uint8_t* bufferBase = buffer->getFrameBufferBase();

    if (!bufferBase)
    {
        logError("Invalid SHM buffer");
        return;
    }

    std::string path =
        storage->buildPath(event.startFrame, event.endFrame, event.trigger);

    logInfo("EVENT START | " + event.trigger);

    cv::Mat prevFrame;
    bool opened = false;

    auto tick = std::chrono::steady_clock::now();

    for (int i = 0; i < (event.endFrame - event.startFrame) && !stop; ++i)
    {
        tick += std::chrono::microseconds(1000000 / config.fps);

        // SAFE SINGLE FRAME READ
        cv::Mat raw(1, SHM_SIZE, CV_8UC1, bufferBase);
        cv::Mat frame = cv::imdecode(raw, cv::IMREAD_COLOR);

        if (frame.empty())
            continue;

        cv::Mat safeFrame = frame.clone();

        auto decision = orchestrator->compute(
            safeFrame,
            prevFrame,
            event.startFrame + i,
            (event.startFrame + event.endFrame) >> 1,
            event.trigger
        );

        if (decision.dropFrame && decision.importance < 0.15f)
        {
            prevFrame = safeFrame;
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

        encoder->writeFrame(safeFrame);
        prevFrame = safeFrame;

        float pressure = governor->computePressure();

        std::this_thread::sleep_until(
            tick + std::chrono::microseconds((int)(pressure * 4000))
        );
    }

    if (opened && encoder->isOpen())
        encoder->close();

    logInfo("EVENT END | saved=" + path);
}