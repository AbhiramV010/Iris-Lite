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
    policy = std::make_unique<CompressionPolicy>();

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

    logInfo("CompressionEngine READY (PELICAN + Stable CRF + Temporal Smoothing)");
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
    std::string path =
        storage->buildPath(event.startFrame, event.endFrame, event.trigger);

    logInfo("EVENT START | " + event.trigger);

    bool opened = false;
    cv::Mat prevFrame;

    const uint64_t start = event.startFrame;
    const uint64_t end = event.endFrame;

    // ---------------- STABILITY STATE ----------------
    float smoothedImportance = 0.0f;
    float smoothedCRF = 28.0f;

    constexpr float IMPORTANCE_ALPHA = 0.15f;
    constexpr float CRF_ALPHA = 0.10f;

    for (uint64_t i = start; i <= end && !stop; ++i)
    {
        uint64_t safeIndex = i % FRAME_BUFFER_SIZE;

        std::vector<uint8_t> jpeg;
        uint32_t size = 0;

        if (!buffer->getFrame(safeIndex, jpeg, size))
            continue;

        cv::Mat frame = cv::imdecode(jpeg, cv::IMREAD_COLOR);
        if (frame.empty())
            continue;

        cv::Mat safeFrame = frame.clone();

        // ---------------- P.E.L.I.C.A.N DECISION ----------------
        auto decision = orchestrator->compute(
            safeFrame,
            prevFrame,
            i,
            (start + end) / 2,
            event.trigger
        );

        float pressure = governor->computePressure();

        // ---------------- TEMPORAL SALIENCY SMOOTHING ----------------
        float rawImportance =
            decision.importance * 0.6f +
            decision.faceBoost * 0.3f +
            (pressure * 0.1f);

        smoothedImportance =
            IMPORTANCE_ALPHA * rawImportance +
            (1.0f - IMPORTANCE_ALPHA) * smoothedImportance;

        // ---------------- DROP LOGIC (STABILIZED) ----------------
        bool shouldDrop =
            decision.dropFrame && smoothedImportance < 0.18f;

        if (shouldDrop || policy->shouldSkipFrame(smoothedImportance, pressure))
        {
            prevFrame = safeFrame;
            continue;
        }

        // ---------------- CRF STABILIZATION (EVENT-WEIGHTED CURVE) ----------------
        float targetCRF =
            policy->computeCRF(smoothedImportance);

        // pressure pushes compression harder
        targetCRF += pressure * 6.0f;

        // clamp for stability
        targetCRF = std::clamp(targetCRF, 18.0f, 38.0f);

        smoothedCRF =
            CRF_ALPHA * targetCRF +
            (1.0f - CRF_ALPHA) * smoothedCRF;

        // ---------------- ENCODER INIT ----------------
        if (!opened)
        {
            if (!encoder->open(path, (int)smoothedCRF))
            {
                logError("Encoder failed");
                return;
            }
            opened = true;
        }

        // ---------------- WRITE FRAME ----------------
        encoder->writeFrame(safeFrame);
        prevFrame = safeFrame;

        // ---------------- ADAPTIVE TIMING ----------------
        std::this_thread::sleep_for(
            std::chrono::microseconds((int)(pressure * 4000))
        );
    }

    if (opened)
        encoder->close();

    logInfo("EVENT END | saved=" + path);
}