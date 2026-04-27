#include "CompressionEngine.hpp"
#include "logging.hpp"
#include <opencv2/opencv.hpp>
#include <chrono>

bool CompressionEngine::initialize(const Config& cfg)
{
    config = cfg;

    policy = std::make_unique<CompressionPolicy>();
    buffer = std::make_unique<SharedFrameBuffer>();
    governor = std::make_unique<SystemGovernor>();

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

    worker = std::thread(&CompressionEngine::workerLoop, this);

    return true;
}

void CompressionEngine::enqueueEvent(const EventWindow& event)
{
    std::lock_guard<std::mutex> lock(mtx);

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

float CompressionEngine::computeTemporalWeight(uint64_t frame, uint64_t peak)
{
    float dist = std::abs((int64_t)frame - (int64_t)peak);

    // gaussian-like decay
    return std::exp(-(dist * dist) / 200.0f);
}

FrameImportance CompressionEngine::evaluateFrame(uint64_t index, const EventWindow& event)
{
    std::vector<uint8_t> jpeg;
    if (!buffer->getFrame(index, jpeg))
        return { 0.0f, false };

    cv::Mat frame = cv::imdecode(jpeg, cv::IMREAD_COLOR);
    if (frame.empty())
        return { 0.0f, false };

    cv::Mat gray;
    cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);

    float motion = 0.0f;
    float spatial = cv::mean(cv::abs(gray))[0] / 255.0f;

    float temporal = computeTemporalWeight(index, (event.startFrame + event.endFrame) / 2);

    float score = policy->importanceScore(
        motion,
        spatial,
        temporal
    );

    bool isPeak = score > 0.78f;

    return { score, isPeak };
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

        processEvent(event);
    }
}

void CompressionEngine::processEvent(const EventWindow& event)
{
    std::string path = "/tmp/iris_" + std::to_string(event.startFrame) + ".mp4";

    bool opened = false;

    uint64_t peakFrame = (event.startFrame + event.endFrame) / 2;

    for (uint64_t i = event.startFrame; i < event.endFrame && !stop; i++)
    {
        FrameImportance imp = evaluateFrame(i, event);

        int crf = policy->computeCRF(imp.score);
        int fps = policy->computeFPS(imp.score);

        if (!opened)
        {
            encoder->open(path, crf);
            opened = true;
        }

        if (imp.score < 0.15f)
            continue; // background skip (safe)

        std::vector<uint8_t> jpeg;
        if (!buffer->getFrame(i, jpeg))
            continue;

        cv::Mat frame = cv::imdecode(jpeg, cv::IMREAD_COLOR);
        if (frame.empty())
            continue;

        encoder->writeFrame(frame);
    }

    encoder->close();
}