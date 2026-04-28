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
}    while (!stop)
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

    uint64_t start = wrap(event.startFrame, FRAME_BUFFER_SIZE);
    uint64_t end   = wrap(event.endFrame, FRAME_BUFFER_SIZE);

    if (start == end)
        end = (start + 1) % FRAME_BUFFER_SIZE;

    cv::Mat prevFrame;
    bool encoderOpen = false;

    int frameCounter = 0;
    int keyframeInterval = config.fps; // 1 second guarantee

    uint64_t i = start;

    do
    {
        std::vector<uint8_t> jpeg;
        bool ok = buffer->getFrame(i, jpeg);

        cv::Mat frame;

        if (!ok || jpeg.empty())
        {
            if (!prevFrame.empty())
                frame = prevFrame.clone(); // temporal hold
            else
            {
                i = (i + 1) % FRAME_BUFFER_SIZE;
                continue;
            }
        }
        else
        {
            frame = cv::imdecode(jpeg, cv::IMREAD_COLOR);
            if (frame.empty())
            {
                if (!prevFrame.empty())
                    frame = prevFrame.clone();
                else
                {
                    i = (i + 1) % FRAME_BUFFER_SIZE;
                    continue;
                }
            }
        }

        // ---------------- MOTION ----------------
        float motion = 0.0f;
        if (!prevFrame.empty())
        {
            cv::Mat diff;
            cv::absdiff(frame, prevFrame, diff);
            motion = cv::mean(diff)[0] / 255.0f;
        }

        lastMotion = 0.85f * lastMotion + 0.15f * motion;

        // ---------------- SPATIAL ----------------
        cv::Mat gray, edges;
        cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
        cv::Canny(gray, edges, 50, 150);

        float spatial =
            static_cast<float>(cv::countNonZero(edges)) /
            (frame.rows * frame.cols + 1e-6f);

        // ---------------- SENSOR (fallback constant) ----------------
        float sensorBoost = 0.5f;

        float importance = policy->importanceScore(
            policy->motion(lastMotion),
            policy->spatial(spatial),
            sensorBoost
        );

        lastScore = 0.85f * lastScore + 0.15f * importance;

        int crf = policy->computeCRF(lastScore);

        // ---------------- FRAME SKIP LOGIC ----------------
        bool isKeyframe = (frameCounter % keyframeInterval == 0);

        bool shouldWrite =
            isKeyframe ||             // guarantee continuity
            importance > 0.35f ||     // motion / entropy
            motion > 0.08f;           // sudden change

        if (!encoderOpen)
        {
            if (!encoder->open(path, crf))
            {
                logError("Encoder open failed");
                return;
            }
            encoderOpen = true;
        }

        if (shouldWrite)
        {
            encoder->writeFrame(frame);
        }

        prevFrame = frame;
        frameCounter++;

        // prevent CPU spikes on Pi
        std::this_thread::sleep_for(std::chrono::milliseconds(2));

        i = (i + 1) % FRAME_BUFFER_SIZE;

    } while (i != end && !stop);

    encoder->close();
}void CompressionEngine::enqueueEvent(const EventWindow& event)
{
    std::lock_guard<std::mutex> lock(mtx);

    if (queue.size() > 100)
    {
        queue.pop();
        logWarn("Compression queue overflow - dropping oldest event");
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

static inline uint64_t normalizeIndex(uint64_t idx, uint64_t bufferSize)
{
    return bufferSize ? (idx % bufferSize) : idx;
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

    cv::Mat prevFrame;
    bool encoderOpen = false;

    // FIX: safe output path (required for competition reproducibility)
    std::string path =
        "/mnt/clipDrive/clips/iris_" +
        std::to_string(event.startFrame) + "_" +
        std::to_string(event.endFrame) + ".mp4";

    uint64_t start = event.startFrame;
    uint64_t end   = event.endFrame;

    // FIX: normalize + enforce ordering
    start = normalizeIndex(start, FRAME_BUFFER_SIZE);
    end   = normalizeIndex(end, FRAME_BUFFER_SIZE);

    if (end <= start)
        end = start + 1;

    for (uint64_t i = start; i != end && !stop; i = (i + 1) % FRAME_BUFFER_SIZE)
    {
        std::vector<uint8_t> jpeg;

        bool ok = buffer->getFrame(i, jpeg);

        cv::Mat frame;

        // FIX: NO silent skipping (prevents missing event data loss)
        if (!ok || jpeg.empty())
        {
            if (!prevFrame.empty())
            {
                frame = prevFrame.clone();  // temporal hold repair
            }
            else
            {
                continue;
            }
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

        // ---------------- MOTION ----------------
        float motion = 0.0f;

        if (!prevFrame.empty())
        {
            cv::Mat diff;
            cv::absdiff(frame, prevFrame, diff);
            motion = cv::mean(diff)[0] / 255.0f;
        }

        lastMotion = 0.85f * lastMotion + 0.15f * motion;

        // ---------------- SPATIAL ----------------
        cv::Mat gray, edges;
        cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
        cv::Canny(gray, edges, 50, 150);

        float spatial = static_cast<float>(cv::countNonZero(edges)) /
                        (frame.rows * frame.cols + 1e-6f);

        // ---------------- SENSOR BOOST HOOK ----------------
        // (safe default = 0.5, since Python mapping is unknown)
        float sensorBoost = 0.5f;

        float importance = policy->importanceScore(
            policy->motion(lastMotion),
            policy->spatial(spatial),
            sensorBoost
        );

        lastScore = 0.85f * lastScore + 0.15f * importance;

        int crf = policy->computeCRF(lastScore);

        // ---------------- ENCODER SAFETY ----------------
        if (!encoderOpen)
        {
            if (!encoder->open(path, crf))
            {
                logError("Failed to open encoder");
                return;
            }
            encoderOpen = true;
        }

        if (!frame.empty())
        {
            encoder->writeFrame(frame);
        }

        prevFrame = frame;

        // FIX: prevents ffmpeg starvation under burst events
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    encoder->close();
}
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
