#include "CompressionEngine.hpp"
#include "logging.hpp"
#include "ImportanceSignal.hpp"

#include <opencv2/opencv.hpp>
#include <algorithm>

CompressionEngine::CompressionEngine(const Config& cfg_)
    : cfg(cfg_)
{
}

CompressionEngine::~CompressionEngine()
{
    shutdown();
}

// ---------------- INIT ----------------

bool CompressionEngine::initialize()
{
    logInfo("IRIS-Lite Compression Engine initializing (Pi-optimized mode)");

    importanceEngine = std::make_unique<ImportanceEngine>(
        cfg.perceptualWidth,
        cfg.perceptualHeight,
        cfg
    );

    policy = std::make_unique<CompressionPolicy>();

    encoder = std::make_unique<H264Encoder>(
        cfg.encodeWidth,
        cfg.encodeHeight,
        cfg.fps,
        cfg.crf,
        true
    );

    running = true;

    logInfo("Engine initialized (stable perceptual pipeline)");
    return true;
}

// ---------------- PROCESS VIDEO ----------------

void CompressionEngine::processVideoFile(const std::string& path)
{
    cv::VideoCapture cap(path);

    if (!cap.isOpened())
    {
        logError("Failed to open video: " + path);
        return;
    }

    if (!encoder->open("output.mp4"))
    {
        logError("Failed to open encoder output");
        return;
    }

    cv::Mat frame;
    cv::Mat prevFrame;

    uint64_t idx = 0;
    uint64_t written = 0;

    // ---------------- TEMPORAL STATE ----------------
    float importanceState = 0.5f;   // global perceptual memory
    float momentum = 0.5f;          // stabilizer (prevents flicker)

    while (cap.read(frame))
    {
        if (frame.empty())
            continue;

        // ---------------- IMPORTANCE ANALYSIS ----------------
        ImportanceSignal signal;

        if (prevFrame.empty())
        {
            signal = importanceEngine->analyze(frame, frame);
        }
        else
        {
            signal = importanceEngine->analyze(frame, prevFrame);
        }

        // ---------------- GLOBAL IMPORTANCE FUSION ----------------
        float instantImportance =
            0.5f * signal.motion +
            0.3f * signal.edges +
            0.2f * signal.faces;

        // ---------------- TEMPORAL SMOOTHING ----------------
        importanceState =
            0.90f * importanceState +
            0.10f * instantImportance;

        importanceState = std::clamp(importanceState, 0.0f, 1.0f);

        // ---------------- MOMENTUM FILTER ----------------
        momentum =
            0.85f * momentum +
            0.15f * importanceState;

        momentum = std::clamp(momentum, 0.0f, 1.0f);

        // ---------------- POLICY ----------------
        float keepProb = policy->computeKeepProbability(momentum, idx);
        float compression = policy->computeCompressionStrength(momentum);

        // adaptive decision boundary
        float threshold = 0.45f + (compression * 0.25f);

        bool keepFrame = (keepProb > threshold);

        // ---------------- ENCODING ----------------
        if (keepFrame)
        {
            if (!encoder->writeFrame(frame))
            {
                logError("Encoder write failure — stopping stream");
                break;
            }

            written++;
        }

        // ---------------- UPDATE STATE ----------------
        prevFrame = frame;
        idx++;
    }

    encoder->close();

    logInfo("Processing complete");
    logInfo("Frames processed: " + std::to_string(idx));
    logInfo("Frames written: " + std::to_string(written));
}

// ---------------- SHUTDOWN ----------------

void CompressionEngine::shutdown()
{
    if (!running)
        return;

    running = false;

    if (encoder)
        encoder->close();

    logInfo("CompressionEngine shutdown complete");
}