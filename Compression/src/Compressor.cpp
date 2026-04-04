#include "Compressor.hpp"
#include "Utils.hpp"
#include <opencv2/imgproc.hpp>
#include <cstdio>
#include <sstream>
#include "Config.hpp"
#include "logging.hpp"

static constexpr size_t MAX_QUEUE_SIZE = 30;        // protect RAM on Pi
static constexpr float  LOW_ACTIVITY_THRESHOLD = 0.05f; // mean importance
static constexpr int    LOW_ACTIVITY_SKIP_FACTOR = 4;   // keep 1 of 4 low-activity frames

Compressor::Compressor(const Config& cfg_)
    : cfg(cfg_)
{
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

    std::string cmd = buildFFmpegCommand(
        outputPath,
        cfg.encodeWidth,
        cfg.encodeHeight,
        cfg.crf,
        cfg.mode
    );

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

    processingThread = std::thread(&Compressor::processingLoop, this);
    encodingThread = std::thread(&Compressor::encodingLoop, this);

    logInfo("Started asynchronous compressor");
    return true;
}

void Compressor::stop()
{
    if (!running)
        return;

    running = false;
    queueCV.notify_all();

    if (processingThread.joinable())
        processingThread.join();

    if (encodingThread.joinable())
        encodingThread.join();

    if (ffmpegPipe)
    {
#ifdef _WIN32
        _pclose(ffmpegPipe);
#else
        pclose(ffmpegPipe);
#endif
        ffmpegPipe = nullptr;
    }

    logInfo("Stopped compressor");
}

void Compressor::pushFrame(const FrameInfo& frame)
{
    enqueueFrame(frame.frame.clone());
}

void Compressor::enqueueFrame(const cv::Mat& frame)
{
    std::lock_guard<std::mutex> lock(queueMutex);

    if (frameQueue.size() >= MAX_QUEUE_SIZE)
    {
        logWarn("Frame queue full, dropping frame to protect memory");
        return;
    }

    frameQueue.push(frame);
    queueCV.notify_one();
}

cv::Mat Compressor::dequeueFrame()
{
    std::unique_lock<std::mutex> lock(queueMutex);

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
    logInfo("Processing thread started");

    int lowActivityCounter = 0;

    while (running)
    {
        cv::Mat raw = dequeueFrame();
        if (raw.empty())
            continue;

        // Downscale for perceptual analysis
        cv::Mat small;
        cv::resize(raw, small, cv::Size(cfg.perceptualWidth, cfg.perceptualHeight));

        // Importance map (motion, edges, contrast, faces, etc.)
        ImportanceMap faceMap = faceDetector->detect(small);
        ImportanceMap imp = importanceGen->compute(small, faceMap);

        // Compute mean importance as a proxy for "how much is happening"
        cv::Scalar meanVal = cv::mean(imp);
        float meanImportance = static_cast<float>(meanVal[0]);

        bool lowActivity = (meanImportance < LOW_ACTIVITY_THRESHOLD);

        if (lowActivity)
        {
            lowActivityCounter++;

            // Skip most frames when nothing is happening
            if (lowActivityCounter % LOW_ACTIVITY_SKIP_FACTOR != 0)
            {
                logDebug("Skipping low-activity frame (mean importance = " +
                    std::to_string(meanImportance) + ")");
                continue;
            }
        }
        else
        {
            // Reset counter when activity resumes
            lowActivityCounter = 0;
        }

        // Apply privacy mask on full-res frame
        privacyMask->apply(raw);

        // Perceptual blending (sharp where important, smooth where not)
        cv::Mat processed = frameProcessor->process(raw, imp);

        // Send to encoder queue
        enqueueFrame(processed);
    }

    logInfo("Processing thread exiting");
}

void Compressor::encodingLoop()
{
    logInfo("Encoding thread started");

    while (running || !frameQueue.empty())
    {
        cv::Mat frame = dequeueFrame();
        if (frame.empty())
            continue;

        if (!ffmpegPipe)
        {
            logError("FFmpeg pipe is null during encoding");
            break;
        }

        size_t bytes = frame.total() * frame.elemSize();
        size_t written = fwrite(frame.data, 1, bytes, ffmpegPipe);

        if (written != bytes)
        {
            logError("Short write to FFmpeg pipe, possible encoder failure");
            break;
        }
    }

    logInfo("Encoding thread exiting");
}
