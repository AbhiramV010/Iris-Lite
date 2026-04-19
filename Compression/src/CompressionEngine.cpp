#include "CompressionEngine.hpp"
#include "logging.hpp"
#include "TriggerEngine.hpp"
#include "ImportanceSignal.hpp"

#include <opencv2/opencv.hpp>

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
    logInfo("Engine initializing");

    importanceEngine = std::make_unique<ImportanceEngine>(
        cfg.perceptualWidth,
        cfg.perceptualHeight,
        cfg
    );
    importanceMap = std::make_unique<ImportanceMap>();
    policy = std::make_unique<CompressionPolicy>();
    controller = std::make_unique<AdaptiveEncoderController>();
    triggerEngine = std::make_unique<TriggerEngine>();

    encoder = std::make_unique<H264Encoder>(
        cfg.encodeWidth,
        cfg.encodeHeight,
        15,
        cfg.crf,
        true
    );

    running = true;

    logInfo("Engine initialized (Layered pipeline active)");
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

    cv::Mat frame;
    cv::Mat prevFrame;

    uint64_t idx = 0;
    uint64_t written = 0;

    encoder->open("output.mp4");

    while (cap.read(frame))
    {
        if (frame.empty())
            continue;

        // ================= FAST PATH (Layer 1) =================
        auto trig = triggerEngine->evaluate(frame, prevFrame);

        // If NOT important → bypass heavy compute
        if (!trig.triggerDeepAnalysis)
        {
            encoder->writeFrame(frame);
            written++;

            prevFrame = frame.clone();
            idx++;
            continue;
        }

        // ================= DEEP PATH (Layer 2) =================
        ImportanceSignal signal;

        auto result = importanceEngine->analyze(frame, prevFrame);

        signal.motion = result.motion;
        signal.edges = result.edges;
        signal.faces = result.faces;
        signal.global = result.global;
        signal.confidence = 1.0f;

        // ================= GLOBAL STATE =================
        importanceMap->update(signal);
        importanceMap->decay(0.98f);

        float globalScore = importanceMap->getGlobal();

        // ================= POLICY DECISION =================
        float keepProb = policy->computeKeepProbability(globalScore, idx);

        if (keepProb > 0.5f)
        {
            encoder->writeFrame(frame);
            written++;
        }

        prevFrame = frame.clone();
        idx++;
    }

    encoder->close();

    logInfo("Processing complete");
    logInfo("Frames: " + std::to_string(idx));
    logInfo("Written: " + std::to_string(written));
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