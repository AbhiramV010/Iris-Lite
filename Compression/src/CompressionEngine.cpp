#include "CompressionEngine.hpp"
#include "logging.hpp"
#include "SharedMemoryConfig.hpp"

#include <opencv2/opencv.hpp>
#include <chrono>
#include <thread>
<<<<<<< Updated upstream
#include <algorithm>
=======
>>>>>>> Stashed changes

bool CompressionEngine::initialize(const Config& cfg)
{
    config = cfg;

    policy = std::make_unique<CompressionPolicy>();
    governor = std::make_unique<SystemGovernor>();
    buffer = std::make_unique<SharedFrameBuffer>();

    if (!buffer->initialize())
    {
        logError("SharedFrameBuffer init failed");
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

    logInfo("CompressionEngine initialized");
    return true;
}

void CompressionEngine::enqueueEvent(const EventWindow& event)
{
    std::lock_guard<std::mutex> lock(mtx);
<<<<<<< Updated upstream

    if (queue.size() > 100)
    {
        queue.pop();
        logWarn("Event queue overflow - dropping oldest");
    }

=======
>>>>>>> Stashed changes
    queue.push(event);
    cv.notify_one();
}

void CompressionEngine::shutdown()
{
    stop = true;
    cv.notify_all();

    if (worker.joinable())
        worker.join();

    if (encoder)
        encoder->close();
}

float CompressionEngine::getPressureThrottle() const
{
<<<<<<< Updated upstream
    return governor ? governor->computePressure() : 0.0f;
=======
    float dist = std::abs((int64_t)frame - (int64_t)peak);
    return std::exp(-(dist * dist) / 80.0f); // tighter focus
>>>>>>> Stashed changes
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
                return;

            event = queue.front();
            queue.pop();
        }

        // ───── pressure ONLY affects scheduling ─────
        if (getPressureThrottle() > 0.85f)
            std::this_thread::sleep_for(std::chrono::milliseconds(60));

        processEvent(event);
    }
}

FrameImportance CompressionEngine::evaluateFrame(uint64_t index, const EventWindow& event)
{
    float temporal = computeTemporalWeight(
        index,
        (event.startFrame + event.endFrame) / 2
    );

    float motion = std::clamp(lastMotion, 0.0f, 1.0f);

    float spatial = 0.5f; // safe baseline (can be upgraded later with edges)

    float importance = policy->importanceScore(
        motion,
        spatial,
        temporal
    );

    bool peak = importance > 0.75f;

    return { importance, peak };
}

float CompressionEngine::computeTemporalWeight(uint64_t frame, uint64_t peak)
{
    float dist = std::abs((int64_t)frame - (int64_t)peak);
    return std::exp(-dist / 120.0f); // smooth perceptual falloff
}

void CompressionEngine::processEvent(const EventWindow& event)
{
    if (!buffer || !encoder)
        return;

    std::string path =
        "/mnt/clipDrive/clips/iris_" +
        std::to_string(event.startFrame) + "_" +
        std::to_string(event.endFrame) + ".mp4";

    cv::Mat prevFrame;
    bool opened = false;

<<<<<<< Updated upstream
    for (uint64_t i = event.startFrame; i < event.endFrame && !stop; ++i)
=======
    float lastImportance = 0.5f;

    uint64_t peakFrame = (event.startFrame + event.endFrame) / 2;

    for (uint64_t i = event.startFrame; i < event.endFrame && !stop; i++)
>>>>>>> Stashed changes
    {
        uint64_t slot = i % FRAME_BUFFER_SIZE;

        std::vector<uint8_t> jpeg;
<<<<<<< Updated upstream
        if (!buffer->getFrame(slot, jpeg))
            continue;

        cv::Mat frame = cv::imdecode(jpeg, cv::IMREAD_COLOR);
        if (frame.empty())
            continue;

        // ───── MOTION ─────
=======
        bool ok = buffer->getFrame(slot, jpeg);

        cv::Mat frame;

        // -------- FRAME RECOVERY (CRITICAL) --------
        if (!ok || jpeg.empty())
        {
            if (!prevFrame.empty())
                frame = prevFrame.clone();
            else
                continue;
        }
        else
        {
            frame = cv::imdecode(jpeg, cv::IMREAD_COLOR);

            if (frame.empty())
            {
                if (!prevFrame.empty())
                    frame = prevFrame.clone();
                else
                    continue;
            }
        }

        // -------- MOTION --------
>>>>>>> Stashed changes
        float motion = 0.0f;

        if (!prevFrame.empty())
        {
            cv::Mat diff;
            cv::absdiff(frame, prevFrame, diff);
            motion = cv::mean(diff)[0] / 255.0f;
        }

<<<<<<< Updated upstream
        lastMotion = 0.85f * lastMotion + 0.15f * motion;

        // ───── IMPORTANCE ─────
        FrameImportance imp = evaluateFrame(i, event);
        lastImportance = 0.85f * lastImportance + 0.15f * imp.score;

        int crf = policy->computeCRF(lastImportance);

        // ───── ENCODER ─────
=======
        // -------- SALIENCY (LIGHTWEIGHT) --------
        cv::Mat gray;
        cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);

        cv::Mat small;
        cv::resize(gray, small, cv::Size(64, 36));

        float saliency = cv::mean(small)[0] / 255.0f;

        // -------- SPATIAL (EDGES) --------
        cv::Mat edges;
        cv::Canny(gray, edges, 50, 150);

        float spatial = static_cast<float>(cv::countNonZero(edges)) /
            (frame.rows * frame.cols + 1e-6f);

        float edgeBoost = std::min(1.0f, spatial * 1.5f);

        // -------- TEMPORAL --------
        float temporal = computeTemporalWeight(i, peakFrame);

        // -------- IMPORTANCE --------
        float importance = policy->importanceScore(
            0.6f * motion + 0.4f * saliency,
            spatial,
            temporal
        );

        importance += 0.15f * edgeBoost;
        importance = std::clamp(importance, 0.0f, 1.0f);

        // -------- SMOOTHING --------
        lastImportance = 0.92f * lastImportance + 0.08f * importance;

        int crf = policy->computeCRF(lastImportance);

        // -------- ENCODER --------
>>>>>>> Stashed changes
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
<<<<<<< Updated upstream
        prevFrame = frame;

=======

        prevFrame = frame;

        // prevent ffmpeg choking
>>>>>>> Stashed changes
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    encoder->close();

<<<<<<< Updated upstream
    logInfo("Event encoded: " + event.trigger);
=======
    logInfo("Clip saved: " + path);
>>>>>>> Stashed changes
}