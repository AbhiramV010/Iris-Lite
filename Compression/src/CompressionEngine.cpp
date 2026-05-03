#include "CompressionEngine.hpp"
#include "logging.hpp"
#include "SharedMemoryConfig.hpp"

#include <opencv2/opencv.hpp>
#include <chrono>
#include <thread>
#include <algorithm>

// --------------------------------------------------
// INIT
// --------------------------------------------------

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

    logInfo("CompressionEngine READY (ROBUST MODE)");
    return true;
}

// --------------------------------------------------
// EVENT INPUT (ROBUST + NO SILENT LOSS)
// --------------------------------------------------

void CompressionEngine::enqueueEvent(const EventWindow& event)
{
    std::string trigger = event.trigger;

    // FIX: never allow empty/unknown labels
    if (trigger.empty() || trigger == "unknown")
        trigger = "motion_event";

    logInfo("EVENT IN | " + trigger);

    cluster.add(event);

    auto events = cluster.flush();

    if (events.empty())
        events.push_back(event);

    {
        std::lock_guard<std::mutex> lock(mtx);

        for (auto& e : events)
        {
            if (e.trigger.empty() || e.trigger == "unknown")
                e.trigger = trigger;

            queue.push(e);
        }
    }

    cv.notify_one();

    logInfo("QUEUE PUSHED | size=" + std::to_string(queue.size()));
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

    if (encoder && encoder->isOpen())
        encoder->close();

    logInfo("Shutdown complete");
}

// --------------------------------------------------
// WORKER THREAD
// --------------------------------------------------

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

// --------------------------------------------------
// CORE PIPELINE (STABLE + SAFE)
// --------------------------------------------------

void CompressionEngine::processEvent(const EventWindow& event)
{
    uint8_t* bufferBase = buffer->getFrameBufferBase();
    uint32_t* sizes = buffer->getFrameSizes();
    uint64_t* headTail = buffer->getHeadTail();

    if (!bufferBase || !sizes || !headTail)
    {
        logError("Invalid SHM pointers");
        return;
    }

    const int head = static_cast<int>(headTail[0]);

    std::string trigger = event.trigger;
    if (trigger.empty() || trigger == "unknown")
        trigger = "motion_event";

    std::string path =
        storage->buildPath(event.startFrame, event.endFrame, trigger);

    logInfo("EVENT START | " + trigger);

    cv::Mat prevFrame;
    bool opened = false;

    auto tick = std::chrono::steady_clock::now();

    int range = static_cast<int>(event.endFrame - event.startFrame);
    range = std::max(1, range);

    for (int i = 0; i < range && !stop; ++i)
    {
        tick += std::chrono::microseconds(1000000 / config.fps);

        int index = (head - (range - i)) % FRAME_BUFFER_SIZE;
        if (index < 0)
            index += FRAME_BUFFER_SIZE;

        if (index < 0 || index >= FRAME_BUFFER_SIZE)
            continue;

        uint32_t size = sizes[index];

        if (size == 0 || size > SLOT_SIZE || size > 5 * 1024 * 1024)
            continue;

        uint8_t* jpegPtr = bufferBase + (index * SLOT_SIZE);

        cv::Mat raw(1, size, CV_8UC1, jpegPtr);
        cv::Mat frame = cv::imdecode(raw, cv::IMREAD_COLOR);

        if (frame.empty())
            continue;

        cv::Mat safeFrame = frame.clone();

        auto decision = orchestrator->compute(
            safeFrame,
            prevFrame,
            event.startFrame + i,
            (event.startFrame + event.endFrame) >> 1,
            trigger
        );

        // SAFE DROP: never allow full silence
        if (decision.dropFrame && decision.importance < 0.12f)
        {
            prevFrame = safeFrame;
            continue;
        }

        if (!opened)
        {
            if (!encoder->open(path, decision.crf))
            {
                logError("Encoder failed to open");
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