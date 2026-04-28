#include "CompressionEngine.hpp"
#include "logging.hpp"
#include "EventTypes.hpp"

#include <opencv2/opencv.hpp>
#include <chrono>
#include <thread>
#include <algorithm>

#include "SharedMemoryConfig.hpp"

bool CompressionEngine::initialize(const Config& cfg)
{
    config = cfg;

    policy = std::make_unique<CompressionPolicy>();
    buffer = std::make_unique<SharedFrameBuffer>();
    governor = std::make_unique<SystemGovernor>();

    logInfo("Initializing SharedFrameBuffer (waiting up to 20s)...");

    if (!buffer->initialize())
    {
        logError("SharedFrameBuffer init failed — producer not running?");
        return false;
    }

    if (!buffer->isValid())
    {
        logError("SharedFrameBuffer mapped but invalid");
        return false;
    }

    encoder = std::make_unique<H264Encoder>(
        cfg.encodeWidth,
        cfg.encodeHeight,
        cfg.fps,
        true
    );

    stop = false;
    worker = std::thread(&CompressionEngine::workerLoop, this);

    logInfo("CompressionEngine initialized successfully");
    return true;
}

void CompressionEngine::enqueueEvent(const EventWindow& event)
{
    cluster.add(event);

    if (cluster.shouldFlush())
    {
        auto events = cluster.flush();

        {
            std::lock_guard<std::mutex> lock(mtx);
            for (const auto& e : events)
                queue.push(e);
        }

        cv.notify_one();
    }
}

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

float CompressionEngine::computeTemporalWeight(uint64_t frame, uint64_t peak)
{
    float dist = std::abs((int64_t)frame - (int64_t)peak);
    return std::exp(-(dist * dist) / 80.0f);
}

void CompressionEngine::workerLoop()
{
    while (!stop)
    {
        EventWindow event;

        {
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait(lock, [&] { return stop || !queue.empty(); });

            if (stop && queue.empty())
                break;

            event = queue.front();
            queue.pop();
        }

        processEvent(event);
    }
}

void CompressionEngine::processEvent(const EventWindow& event)
{
    if (!buffer || !encoder || !governor)
    {
        logError("Engine not properly initialized");
        return;
    }

    if (!buffer->isValid())
    {
        logError("Shared buffer invalid during processing");
        return;
    }

    std::string path =
        "/mnt/clipDrive/clips/iris_" +
        std::to_string(event.startFrame) + "_" +
        std::to_string(event.endFrame) + ".mp4";

    cv::Mat prevFrame;
    bool opened = false;

    float lastImportance = 0.5f;
    uint64_t peakFrame =
        (event.startFrame + event.endFrame) / 2;

    for (uint64_t i = event.startFrame;
        i < event.endFrame && !stop;
        i++)
    {
        // ---------- GOVERNOR SKIP ----------
        if (governor->shouldSkipFrame(lastImportance))
            continue;

        uint64_t slot = i % FRAME_BUFFER_SIZE;

        std::vector<uint8_t> jpeg;
        bool ok = buffer->getFrame(slot, jpeg);

        cv::Mat frame;

        // ---------- SAFE FRAME RECOVERY ----------
        if (!ok || jpeg.empty())
        {
            if (prevFrame.empty())
                continue;

            frame = prevFrame.clone();
        }
        else
        {
            frame = cv::imdecode(jpeg, cv::IMREAD_COLOR);

            if (frame.empty())
            {
                if (prevFrame.empty())
                    continue;

                frame = prevFrame.clone();
            }
        }

        // ---------- MOTION ----------
        float motion = 0.0f;

        if (!prevFrame.empty())
        {
            cv::Mat diff;
            cv::absdiff(frame, prevFrame, diff);
            motion = cv::mean(diff)[0] / 255.0f;
        }

        // ---------- GRAYSCALE ----------
        cv::Mat gray;
        cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);

        // ---------- SALIENCY ----------
        cv::Mat small;
        cv::resize(gray, small, cv::Size(64, 36));
        float saliency = cv::mean(small)[0] / 255.0f;

        // ---------- SPATIAL ----------
        cv::Mat edges;
        cv::Canny(gray, edges, 50, 150);

        float spatial =
            static_cast<float>(cv::countNonZero(edges)) /
            (frame.rows * frame.cols + 1e-6f);

        // ---------- TEMPORAL ----------
        float temporal = computeTemporalWeight(i, peakFrame);

        // ---------- IMPORTANCE ----------
        float importance = policy->importanceScore(
            0.6f * motion + 0.4f * saliency,
            spatial,
            temporal
        );

        importance = std::clamp(importance, 0.0f, 1.0f);

        lastImportance =
            0.92f * lastImportance +
            0.08f * importance;

        // ---------- ADAPTIVE CRF ----------
        int baseCRF = policy->computeCRF(lastImportance);
        int crf = governor->adaptiveCRF(baseCRF);

        // ---------- ENCODER INIT ----------
        if (!opened)
        {
            if (!encoder->open(path, crf))
            {
                logError("Encoder open failed");
                return;
            }
            opened = true;
        }

        encoder->writeFrame(frame);
        prevFrame = frame;

        // ---------- THROTTLE ----------
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    if (opened)
        encoder->close();

    logInfo("Clip saved: " + path);
}