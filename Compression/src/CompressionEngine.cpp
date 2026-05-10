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

    worker = std::thread(
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
    logInfo(
        "EVENT IN | trigger=" +
        event.trigger +
        " start=" +
        std::to_string(event.startFrame) +
        " end=" +
        std::to_string(event.endFrame)
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

            logInfo(
                "QUEUED EVENT | " +
                e.trigger +
                " [" +
                std::to_string(e.startFrame) +
                " -> " +
                std::to_string(e.endFrame) +
                "]"
            );
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

            cv.wait(lock, [&]
                {
                    return stop || !queue.empty();
                });

            if (stop && queue.empty())
                break;

            event = queue.front();
            queue.pop();

            logInfo(
                "DEQUEUED EVENT | remaining=" +
                std::to_string(queue.size())
            );
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

    logInfo(
        "EVENT START | " +
        event.trigger +
        " | frames " +
        std::to_string(event.startFrame) +
        " -> " +
        std::to_string(event.endFrame)
    );

    bool encoderOpened = false;

    cv::Mat prevFrame;

    const uint64_t start = event.startFrame;
    const uint64_t end = event.endFrame;

    int encodedFrames = 0;
    int droppedFrames = 0;
    int failedFrames = 0;

    for (uint64_t i = start;
        i <= end && !stop;
        ++i)
    {
        const uint64_t index =
            i % BUFFER_SIZE;

        logInfo(
            "READ FRAME INDEX = " +
            std::to_string(index)
        );

        uint32_t frameSize =
            buffer->getFrameSize(index);

        if (frameSize == 0)
        {
            failedFrames++;

            logError(
                "EMPTY FRAME SIZE | index=" +
                std::to_string(index)
            );

            continue;
        }

        if (frameSize > SLOT_SIZE)
        {
            failedFrames++;

            logError(
                "INVALID FRAME SIZE | index=" +
                std::to_string(index) +
                " size=" +
                std::to_string(frameSize)
            );

            continue;
        }

        const uint8_t* frameData =
            buffer->getFrameData(index);

        if (!frameData)
        {
            failedFrames++;

            logError(
                "NULL FRAME DATA | index=" +
                std::to_string(index)
            );

            continue;
        }

        std::vector<uint8_t> jpeg(
            frameData,
            frameData + frameSize
        );

        cv::Mat frame =
            cv::imdecode(
                jpeg,
                cv::IMREAD_COLOR
            );

        if (frame.empty())
        {
            failedFrames++;

            logError(
                "FRAME DECODE FAILED | index=" +
                std::to_string(index)
            );

            continue;
        }

        cv::Mat safeFrame = frame.clone();

        float pressure =
            governor->computePressure();

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
                1.0f,
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

            logInfo(
                "OPENING ENCODER | path=" +
                path
            );

            if (!encoder->open(path, initialCRF))
            {
                logError("Encoder failed to open");
                return;
            }

            encoderOpened = true;
        }

        // --------------------------------------------------
        // ADAPTIVE QUALITY
        // --------------------------------------------------

        encoder->setQuality(
            decision.crf
        );

        encoder->setRegionImportance(
            fusedImportance
        );

        // --------------------------------------------------
        // SMART FRAME DROPPING
        // --------------------------------------------------

        bool shouldDrop =
            decision.dropFrame &&
            pressure > 0.96f &&
            fusedImportance < 0.25f;

        if (shouldDrop)
        {
            droppedFrames++;

            logInfo(
                "FRAME DROPPED | index=" +
                std::to_string(index)
            );

            prevFrame = safeFrame;
            continue;
        }

        // --------------------------------------------------
        // ENCODE FRAME
        // --------------------------------------------------

        bool success =
            encoder->writeFrame(safeFrame);

        if (!success)
        {
            failedFrames++;

            logError(
                "ENCODE FAILED | index=" +
                std::to_string(index)
            );

            continue;
        }

        encodedFrames++;

        prevFrame = safeFrame;

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
    {
        logInfo("Closing encoder");
        encoder->close();
    }

    logInfo(
        "EVENT END | saved=" + path +
        " encoded=" + std::to_string(encodedFrames) +
        " dropped=" + std::to_string(droppedFrames) +
        " failed=" + std::to_string(failedFrames)
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
        worker.join();

    if (encoder && encoder->isOpen())
        encoder->close();

    logInfo("CompressionEngine shutdown complete");
}