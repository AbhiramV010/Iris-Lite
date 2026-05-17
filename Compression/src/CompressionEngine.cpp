#include "CompressionEngine.hpp"
#include "logging.hpp"
#include "SharedMemoryConfig.hpp"

#include <opencv2/opencv.hpp>

#include <chrono>
#include <thread>
#include <algorithm>
#include <vector>

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
        logError(
            "SharedFrameBuffer failed to initialize"
        );

        return false;
    }

    storage->ensureReady();

    stop = false;

    worker = std::thread(
        &CompressionEngine::workerLoop,
        this
    );

    logInfo(
        "CompressionEngine READY"
    );

    return true;
}

// --------------------------------------------------
// EVENT QUEUE
// --------------------------------------------------

void CompressionEngine::enqueueEvent(
    const EventWindow& event
)
{
    logInfo(
        "EVENT IN | " +
        event.trigger +
        " [" +
        std::to_string(event.startFrame) +
        " -> " +
        std::to_string(event.endFrame) +
        "]"
    );

    cluster.add(event);

    auto events = cluster.flush();

    if (events.empty())
        events.push_back(event);

    {
        std::lock_guard<std::mutex> lock(mtx);

        for (const auto& e : events)
        {
            queue.push(e);
        }

        logInfo(
            "QUEUE SIZE = " +
            std::to_string(queue.size())
        );
    }

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

            cv.wait(
                lock,
                [&]
                {
                    return stop || !queue.empty();
                }
            );

            if (stop && queue.empty())
                break;

            event = queue.front();
            queue.pop();
        }

        logInfo(
            "DEQUEUED EVENT | " +
            event.trigger +
            " [" +
            std::to_string(event.startFrame) +
            " -> " +
            std::to_string(event.endFrame) +
            "]"
        );

        processEvent(event);
    }
}

// --------------------------------------------------
// MAIN PIPELINE
// --------------------------------------------------

void CompressionEngine::processEvent(
    const EventWindow& event
)
{
    std::string path = storage->buildPath(
        event.startFrame,
        event.endFrame,
        event.trigger
    );

    logInfo(
        "EVENT START | " +
        event.trigger +
        " frames " +
        std::to_string(event.startFrame) +
        " -> " +
        std::to_string(event.endFrame)
    );

    const uint64_t start = event.startFrame;
    const uint64_t end = event.endFrame;

    bool encoderOpened = false;

    cv::Mat prevFrame;

    int encoded = 0;
    int dropped = 0;
    int failed = 0;

    for (
        uint64_t i = start;
        i <= end && !stop;
        ++i
    )
    {
        std::vector<uint8_t> jpeg;

        if (!buffer->readFrame(i, jpeg))
        {
            failed++;
            continue;
        }

        cv::Mat decoded =
            cv::imdecode(
                jpeg,
                cv::IMREAD_COLOR
            );

        if (decoded.empty())
        {
            failed++;
            continue;
        }

        cv::Mat frame = decoded.clone();

        float pressure =
            governor->computePressure();

        auto decision =
            orchestrator->compute(
                frame,
                prevFrame,
                i,
                (start + end) / 2,
                event.trigger
            );

        float fused =
            policy->fuseImportance(
                decision.importance,
                1.0f,
                pressure
            );

        // ------------------------------------------
        // OPEN ENCODER
        // ------------------------------------------

        if (!encoderOpened)
        {
            int crf =
                policy->computeCRF(fused);

            logInfo(
                "OPEN ENCODER | " + path
            );

            if (!encoder->open(path, crf))
            {
                logError(
                    "Encoder open failed"
                );

                return;
            }

            encoderOpened = true;
        }

        encoder->setQuality(
            decision.crf
        );

        encoder->setRegionImportance(
            fused
        );

        bool shouldDrop =
            decision.dropFrame &&
            pressure > 0.96f &&
            fused < 0.25f;

        if (shouldDrop)
        {
            dropped++;
            prevFrame = frame;
            continue;
        }

        bool success =
            encoder->writeFrame(frame);

        if (!success)
        {
            failed++;

            logError(
                "Encoder write failed"
            );

            break;
        }

        encoded++;

        prevFrame = frame;

        std::this_thread::sleep_for(
            std::chrono::microseconds(
                1000000 / config.fps
            )
        );
    }

    if (encoderOpened)
    {
        encoder->close();
    }

    logInfo(
        "EVENT END | " +
        path +
        " encoded=" +
        std::to_string(encoded) +
        " dropped=" +
        std::to_string(dropped) +
        " failed=" +
        std::to_string(failed)
    );
}

// --------------------------------------------------
// SHUTDOWN
// --------------------------------------------------

void CompressionEngine::shutdown()
{
    stop = true;

    cv.notify_all();

    if (worker.joinable())
    {
        worker.join();
    }

    if (encoder && encoder->isOpen())
    {
        encoder->close();
    }

    logInfo(
        "CompressionEngine shutdown complete"
    );
}