#include "CompressionEngine.hpp"
#include "logging.hpp"
#include "SharedMemoryConfig.hpp"
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
    uint8_t* bufferBase = buffer->getFrameBufferBase();
    uint32_t* sizes = buffer->getFrameSizes();
    uint64_t* headTail = buffer->getHeadTail();

    if (!bufferBase || !sizes || !headTail)
        return;

    const int head = static_cast<int>(headTail[0]);

    std::string path =
        storage->buildPath(event.startFrame, event.endFrame, event.trigger);

    logInfo("EVENT START | " + event.trigger);

    cv::Mat prevFrame;
    bool opened = false;

    auto tick = std::chrono::steady_clock::now();

    const int range = static_cast<int>(event.endFrame - event.startFrame);

    for (int i = 0; i < range && !stop; ++i)
    {
        tick += std::chrono::microseconds(1000000 / config.fps);

        // ---------------- FIXED SAFE RING INDEX ----------------
        int index = (head - (range - i)) % FRAME_BUFFER_SIZE;
        if (index < 0)
            index += FRAME_BUFFER_SIZE;

        uint32_t size = sizes[index];

        if (size == 0 || size > SLOT_SIZE)
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
            event.trigger
        );

        if (decision.dropFrame)
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


