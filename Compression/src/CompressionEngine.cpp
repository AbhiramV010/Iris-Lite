#include "CompressionEngine.hpp"
#include "logging.hpp"
#include "SystemGovernor.cpp"
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
    if (!governor) return 0.0f;
    return governor->computePressure();
}

void CompressionEngine::workerLoop()
{
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
                return;

            event = queue.front();
            queue.pop();
        }

        if (getPressureThrottle() > 0.85f)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

        processEvent(event);
    }
}

void CompressionEngine::processEvent(const EventWindow& event)
{
    cv::Mat prev;
    bool opened = false;

    std::string path = "/tmp/iris_" + std::to_string(event.startFrame) + ".mp4";

    for (uint64_t i = event.startFrame; i < event.endFrame && !stop; i++)
    {
        std::vector<uint8_t> jpeg;
        if (!buffer->getFrame(i, jpeg))
            continue;

        cv::Mat frame = cv::imdecode(jpeg, cv::IMREAD_COLOR);
        if (frame.empty())
            continue;

        float motion = 0.0f;

        if (!prev.empty())
        {
            cv::Mat diff;
            cv::absdiff(frame, prev, diff);
            motion = cv::mean(diff)[0] / 255.0f;
        }

        lastMotion = 0.85f * lastMotion + 0.15f * motion;

        cv::Mat gray, edges;
        cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
        cv::Canny(gray, edges, 50, 150);

        float spatial = (float)cv::countNonZero(edges) /
            (frame.rows * frame.cols + 1e-6f);

        float score = policy->perceptualScore(
            policy->motion(lastMotion),
            policy->spatial(spatial),
            policy->face(false),
            policy->region(0.5f)
        );

        lastScore = 0.85f * lastScore + 0.15f * score;

        int crf = policy->computeCRF(lastScore, config.baseCRF);

        if (!opened)
        {
            encoder->open(path, crf);
            opened = true;
        }

        encoder->writeFrame(frame);

        prev = frame;
    }

    encoder->close();
}
