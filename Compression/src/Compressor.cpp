#include "Compressor.hpp"
#include "Utils.hpp"
#include <cstdio>
#include <sstream>
#include "Config.hpp"
#include "logging.hpp"

// Tuned parameters with ablation study justification
static constexpr size_t MAX_QUEUE_SIZE = 30;           // Protect 1GB Pi RAM
static constexpr float  LOW_ACTIVITY_THRESHOLD = 0.05f; // 5% mean importance threshold
static constexpr int    LOW_ACTIVITY_SKIP_FACTOR = 4;   // Keep 1 of 4 frames when inactive

Compressor::Compressor(const Config& cfg_)
    : cfg(cfg_)
{
    // Instantiate all processing modules with config parameters
    importanceGen = new ImportanceMapGenerator(
        cfg.perceptualWidth,
        cfg.perceptualHeight,
        cfg.useFaces,
        cfg.useMotion,
        cfg.useEdges
    );

    faceDetector = new FaceDetector(
        cfg.perceptualWidth,
        cfg.perceptualHeight,
        cfg.useFaces
    );

    frameProcessor = new FrameProcessor(
        cfg.encodeWidth,
        cfg.encodeHeight
    );

    privacyMask = new PrivacyMask(cfg.privacyZones);
}

Compressor::~Compressor()
{
    stop();

    delete importanceGen;
    delete faceDetector;
    delete frameProcessor;
    delete privacyMask;
}

bool Compressor::start(const std::string& outputPath)
{
    if (running)
        return false;

    running = true;

    // Build platform-specific FFmpeg command
    std::string cmd = buildFFmpegCommand(
        outputPath,
        cfg.encodeWidth,
        cfg.encodeHeight,
        cfg.crf,
        cfg.mode
    );

    // Open FFmpeg process via pipe
#ifdef _WIN32
    ffmpegPipe = _popen(cmd.c_str(), "wb");
#else
    ffmpegPipe = popen(cmd.c_str(), "w");
#endif

    if (!ffmpegPipe)
    {
        logError("Failed to open FFmpeg pipe");
        running = false;
        return false;
    }

    // Start dual-threaded processing: analysis and encoding run in parallel
    processingThread = std::thread(&Compressor::processingLoop, this);
    encodingThread = std::thread(&Compressor::encodingLoop, this);

    logInfo("Compressor started with dual-threaded architecture");
    return true;
}

void Compressor::stop()
{
    if (!running)
        return;

    running = false;
    queueCV.notify_all();

    // Wait for threads to complete
    if (processingThread.joinable())
        processingThread.join();

    if (encodingThread.joinable())
        encodingThread.join();

    // Close FFmpeg process
    if (ffmpegPipe)
    {
#ifdef _WIN32
        _pclose(ffmpegPipe);
#else
        pclose(ffmpegPipe);
#endif
        ffmpegPipe = nullptr;
    }

    logInfo("Compressor stopped successfully");
}

void Compressor::pushFrame(const FrameInfo& frame)
{
    enqueueFrame(frame.frame.clone());
}

void Compressor::enqueueFrame(const cv::Mat& frame)
{
    std::lock_guard<std::mutex> lock(queueMutex);

    // Prevent unbounded queue growth (protect limited RAM on Pi)
    if (frameQueue.size() >= MAX_QUEUE_SIZE)
    {
        logWarn("Frame queue at capacity (" + std::to_string(MAX_QUEUE_SIZE) +
            "), dropping frame to maintain stability");
        return;
    }

    frameQueue.push(frame);
    queueCV.notify_one();  // Wake encoding thread if waiting
}

cv::Mat Compressor::dequeueFrame()
{
    std::unique_lock<std::mutex> lock(queueMutex);

    // Block until frame available or shutdown signaled
    queueCV.wait(lock, [&] {
        return !running || !frameQueue.empty();
        });

    if (frameQueue.empty())
        return cv::Mat();

    cv::Mat f = frameQueue.front();
    frameQueue.pop();
    return f;
}

void Compressor::processingLoop()
{
    logInfo("Processing thread started (importance map generation and blending)");

    int lowActivityCounter = 0;

    while (running)
    {
        cv::Mat raw = dequeueFrame();
        if (raw.empty())
            continue;

        // Downscale for perceptual analysis (36x fewer pixels = faster processing)
        cv::Mat small;
        cv::resize(raw, small, cv::Size(cfg.perceptualWidth, cfg.perceptualHeight));

        // Compute importance map from 4 features (motion, edges, contrast, center bias)
        ImportanceMap faceMap = faceDetector->detect(small);
        ImportanceMap imp = importanceGen->compute(small, faceMap);

        // Evaluate scene activity from mean importance
        cv::Scalar meanVal = cv::mean(imp);
        float meanImportance = static_cast<float>(meanVal[0]);

        bool lowActivity = (meanImportance < LOW_ACTIVITY_THRESHOLD);

        // Adaptive frame skipping: drop frames during low-activity scenes
        if (lowActivity)
        {
            lowActivityCounter++;

            if (lowActivityCounter % LOW_ACTIVITY_SKIP_FACTOR != 0)
            {
                logDebug("Skipped low-activity frame (mean importance: " +
                    std::to_string(meanImportance) + ")");
                continue;  // Don't process this frame
            }
        }
        else
        {
            // Reset counter when activity detected
            lowActivityCounter = 0;
        }

        // Apply privacy masking (irreversible, applied before encoding)
        privacyMask->apply(raw);

        // Perceptual blending: sharp in important regions, smooth elsewhere
        cv::Mat processed = frameProcessor->process(raw, imp);

        // Enqueue processed frame for encoder
        enqueueFrame(processed);
    }

    logInfo("Processing thread exiting");
}

void Compressor::encodingLoop()
{
    logInfo("Encoding thread started (FFmpeg H.264 encoding)");

    int frameCount = 0;

    while (running || !frameQueue.empty())
    {
        cv::Mat frame = dequeueFrame();
        if (frame.empty())
            continue;

        if (!ffmpegPipe)
        {
            logError("FFmpeg pipe null, encoding aborted");
            break;
        }

        // Write raw pixel data to FFmpeg stdin
        size_t bytes = frame.total() * frame.elemSize();
        size_t written = fwrite(frame.data, 1, bytes, ffmpegPipe);

        if (written != bytes)
        {
            logError("Incomplete write to FFmpeg (" + std::to_string(written) +
                "/" + std::to_string(bytes) + " bytes)");
            break;
        }

        frameCount++;
    }

    logInfo("Encoding thread exiting (encoded " + std::to_string(frameCount) + " frames)");
}