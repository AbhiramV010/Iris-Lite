#include "CompressionEngine.hpp"
#include "SharedMemoryConfig.hpp"
#include "logging.hpp"

#include <opencv2/opencv.hpp>
#include <chrono>
#include <iomanip>
#include <sstream>

// ---------------- TIME FIX (NO std::format) ----------------
static std::string timestamp()
{
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);

    std::ostringstream oss;
    oss << std::put_time(std::localtime(&t), "%Y-%m-%d_%H-%M-%S");
    return oss.str();
}

// ---------------- INIT ----------------

bool CompressionEngine::initialize(const Config& cfg)
{
    importance = std::make_unique<ImportanceEngine>(
        cfg.perceptualWidth,
        cfg.perceptualHeight,
        cfg
    );

    policy = std::make_unique<CompressionPolicy>();

    Encoder = std::make_unique<H264Encoder>(
        cfg.encodeWidth,
        cfg.encodeHeight,
        cfg.fps,
        true
    );

    sharedBuffer = std::make_unique<SharedFrameBuffer>();
    if (!sharedBuffer->initialize())
        return false;

    running = true;
    stopWorker = false;

    worker = std::thread(&CompressionEngine::workerLoop, this);

    return true;
}

// ---------------- ENQUEUE ----------------

void CompressionEngine::enqueueEvent(const EventWindow& event)
{
    {
        std::lock_guard<std::mutex> lock(eventMutex);
        eventQueue.push(event);
    }
    cv.notify_one();
}

// ---------------- WORKER LOOP ----------------

void CompressionEngine::workerLoop()
{
    while (!stopWorker)
    {
        EventWindow event;

        {
            std::unique_lock<std::mutex> lock(eventMutex);

            cv.wait(lock, [&] {
                return stopWorker || !eventQueue.empty();
                });

            if (stopWorker)
                return;

            event = eventQueue.front();
            eventQueue.pop();
        }

        processEvent(event);
    }
}

// ---------------- EVENT PROCESSING ----------------

void CompressionEngine::processEvent(const EventWindow& event)
{
    std::string path =
        "/clipDrive/clips/iris_lite--" + timestamp() + ".mp4";

    startNewSegment(path, currentCRF);

    cv::Mat prev;

    for (uint64_t i = event.startFrame; i < event.endFrame; i++)
    {
        std::vector<uint8_t> jpeg;

        if (!sharedBuffer->getFrame(i, jpeg))
            continue;

        cv::Mat frame = cv::imdecode(jpeg, cv::IMREAD_COLOR);
        if (frame.empty())
            continue;

        auto signal = importance->analyze(frame, prev);

        importanceState = 0.9f * importanceState + 0.1f * signal.global;
        momentum = 0.85f * momentum + 0.15f * importanceState;

        bool keep = policy->shouldKeepFrame(
            importanceState,
            momentum,
            i
        );

        if (keep && Encoder->isOpen())
            Encoder->writeFrame(frame);

        prev = frame;
    }

    if (Encoder->isOpen())
        Encoder->close();
}

// ---------------- SEGMENT ----------------

void CompressionEngine::startNewSegment(const std::string& fileName, int crf)
{
    if (Encoder->isOpen())
        Encoder->close();

    if (!Encoder->open(fileName, crf))
        running = false;
}

// ---------------- SHUTDOWN ----------------

void CompressionEngine::shutdown()
{
    stopWorker = true;
    cv.notify_all();

    if (worker.joinable())
        worker.join();

    if (Encoder)
        Encoder->close();
}