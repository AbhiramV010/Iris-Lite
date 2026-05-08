#include "CompressionEngine.hpp"
#include "logging.hpp"
#include "SharedMemoryConfig.hpp"

#include <opencv2/opencv.hpp>
#include <chrono>
#include <thread>

// --------------------------------------------------
// INIT
// --------------------------------------------------

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

    worker =
        std::thread(
            &CompressionEngine::workerLoop,
            this
        );

    logInfo(
        "CompressionEngine READY "
        "(STRICT RING BUFFER MODE)"
    );

    return true;
}

// --------------------------------------------------
// EVENT QUEUE
// --------------------------------------------------

void CompressionEngine::enqueueEvent(
    const EventWindow& event)
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

// --------------------------------------------------
// WORKER LOOP
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
// MAIN PIPELINE
// --------------------------------------------------

void CompressionEngine::processEvent(
    const EventWindow& event)
{
    std::string path =
        storage->buildPath(
            event.startFrame,
            event.endFrame,
            event.trigger
        );

    logInfo("EVENT START | " + event.trigger);

    bool encoderOpened = false;

    cv::Mat prevFrame;

    const uint64_t start = event.startFrame;
    const uint64_t end = event.endFrame;

    for (uint64_t i = start;
         i <= end && !stop;
         ++i)
    {
        // --------------------------------------------------
        // STRICT SHM RING BUFFER ACCESS
        // --------------------------------------------------

        const uint64_t index =
            i % FRAME_BUFFER_SIZE;

        std::vector<uint8_t> jpeg;
        uint32_t size = 0;

        if (!buffer->getFrame(index, jpeg, size))
        {
            continue;
        }

        cv::Mat frame =
            cv::imdecode(
                jpeg,
                cv::IMREAD_COLOR
            );

        if (frame.empty())
            continue;

        cv::Mat safeFrame = frame.clone();

        // --------------------------------------------------
        // SYSTEM PRESSURE
        // --------------------------------------------------

        float pressure =
            governor->computePressure();

        // --------------------------------------------------
        // ORCHESTRATOR DECISION
        // --------------------------------------------------

        auto decision =
            orchestrator->compute(
                safeFrame,
                prevFrame,
                i,
                (start + end) / 2,
                event.trigger
            );

        float fusedImportance =
            policy->fuseImportance(
                decision.importance,
                decision.faceBoost,
                pressure
            );

        // --------------------------------------------------
        // OPEN ENCODER
        // --------------------------------------------------

        if (!encoderOpened)
        {
            int initialCRF =
                policy->computeCRF(
                    fusedImportance
                );

            if (!encoder->open(path, initialCRF))
            {
                logError("Encoder failed");
                return;
            }

            encoderOpened = true;
        }

        // --------------------------------------------------
        // ADAPTIVE QUALITY
        // --------------------------------------------------

        encoder->setQuality(
            (int)decision.crf
        );

        // perceptual weighting
        encoder->setRegionImportance(
            fusedImportance
        );

        encoder->setFaceImportance(
            decision.faceBoost
        );

        // --------------------------------------------------
        // SMART FRAME DROPPING
        // --------------------------------------------------

        bool shouldDrop =
            decision.dropFrame &&
            pressure > 0.92f &&
            fusedImportance < 0.25f;

        // preserve continuity
        if (shouldDrop)
        {
            prevFrame = safeFrame;
            continue;
        }

        // --------------------------------------------------
        // ENCODE FRAME
        // --------------------------------------------------

        encoder->writeFrame(safeFrame);

        prevFrame = safeFrame;

        // --------------------------------------------------
        // TIMING CONTROL
        // --------------------------------------------------

        std::this_thread::sleep_for(
            std::chrono::microseconds(
                1000000 / config.fps
            )
        );
    }

    // --------------------------------------------------
    // CLEANUP
    // --------------------------------------------------

    if (encoderOpened)
        encoder->close();

    logInfo("EVENT END | saved=" + path);
}