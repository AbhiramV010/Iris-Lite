#include "CompressionEngine.hpp"
#include "logging.hpp"
#include "SharedMemoryConfig.hpp"

#include <opencv2/opencv.hpp>
#include <chrono>
#include <thread>

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
        true // try hardware first
    );

    stop = false;
    worker = std::thread(&CompressionEngine::workerLoop, this);

    logInfo("CompressionEngine initialized");
    return true;
}

void CompressionEngine::enqueueEvent(const EventWindow& event)
{
    std::lock_guard<std::mutex> lock(mtx);

    if (queue.size() > 100)
    {
        queue.pop();
        logWarn("Queue overflow drop");
    }

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

float CompressionEngine::getPressureThrottle()
{
    return governor ? governor->computePressure() : 0.0f;
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

        if (getPressureThrottle() > 0.85f)
            std::this_thread::sleep_for(std::chrono::milliseconds(80));

        processEvent(event);
    }
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

    uint64_t frameCount = 0;

    for (uint64_t i = event.startFrame; i < event.endFrame && !stop; ++i)
    {
        uint64_t slot = i % FRAME_BUFFER_SIZE;

        std::vector<uint8_t> jpeg;
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
        float motion = 0.0f;

        if (!prevFrame.empty())
        {
            cv::Mat diff;
            cv::absdiff(frame, prevFrame, diff);
            motion = cv::mean(diff)[0] / 255.0f;
        }

        lastMotion = 0.85f * lastMotion + 0.15f * motion;

        // -------- SPATIAL --------
        cv::Mat gray, edges;
        cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
        cv::Canny(gray, edges, 50, 150);

        float spatial = static_cast<float>(cv::countNonZero(edges)) /
                        (frame.rows * frame.cols + 1e-6f);

        // -------- IMPORTANCE --------
        float importance = policy->importanceScore(
            policy->motion(lastMotion),
            policy->spatial(spatial),
            0.5f // safe default sensor boost
        );

        lastScore = 0.85f * lastScore + 0.15f * importance;

        int crf = policy->computeCRF(lastScore);

        // -------- ENCODER --------
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
        frameCount++;

        // small sleep to avoid ffmpeg overload
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    encoder->close();

    logInfo("Clip saved with frames: " + std::to_string(frameCount));
}
