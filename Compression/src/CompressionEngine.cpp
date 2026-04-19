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
        cfg.fps,
        cfg.crf,
        true
    );

    running = true;

    logInfo("Engine initialized (Layered perceptual pipeline active)");
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

    // ---------------- MEMORY STATES ----------------
    float eventEnergy = 0.0f;
    bool eventActive = false;

    float subjectEnergy = 0.0f;
    bool subjectActive = false;

    float decisionMomentum = 0.5f;
    bool lastKeepDecision = true;

    float bitrateBudget = 1.0f;
    float budgetDecay = 0.995f;

    ImportanceSignal smooth{};
    const float alpha = 0.8f;

    encoder->open("output.mp4");

    while (cap.read(frame))
    {
        if (frame.empty())
            continue;

        // ================= FAST PATH =================
        auto trig = triggerEngine->evaluate(frame, prevFrame);

        // Skip heavy compute if not needed
        if (!trig.triggerDeepAnalysis)
        {
            encoder->writeFrame(frame);
            written++;

            prevFrame = frame.clone();
            idx++;
            continue;
        }

        // ================= DEEP ANALYSIS =================
        auto result = importanceEngine->analyze(frame, prevFrame);

        ImportanceSignal signal;
        signal.motion = result.motion;
        signal.edges = result.edges;
        signal.faces = result.faces;
        signal.global = result.global;
        signal.confidence = 1.0f;

        // ================= SUBJECT MEMORY =================
        subjectEnergy = 0.92f * subjectEnergy + signal.faces;

        subjectActive = (subjectEnergy > 0.25f) || subjectActive;
        if (subjectEnergy < 0.1f)
            subjectActive = false;

        // ================= EVENT MEMORY =================
        eventEnergy = 0.9f * eventEnergy + signal.global;

        eventActive = (eventEnergy > 0.35f) || eventActive;
        if (eventEnergy < 0.15f)
            eventActive = false;

        // ================= SIGNAL SMOOTHING =================
        smooth.motion = alpha * smooth.motion + (1.0f - alpha) * signal.motion;
        smooth.edges = alpha * smooth.edges + (1.0f - alpha) * signal.edges;
        smooth.faces = alpha * smooth.faces + (1.0f - alpha) * signal.faces;
        smooth.global = alpha * smooth.global + (1.0f - alpha) * signal.global;

        const ImportanceSignal& finalSignal = smooth;

        // ================= GLOBAL STATE =================
        importanceMap->update(finalSignal);
        importanceMap->decay(0.98f);

        float globalScore = importanceMap->getGlobal();
        static float sceneEnergy = 0.0f;

        sceneEnergy = 0.98f * sceneEnergy + globalScore;

        bool sceneBreak = (sceneEnergy < 0.08f);

        bitrateBudget += globalScore * 0.05f;
        bitrateBudget = std::clamp(bitrateBudget, 0.2f, 2.0f);
        // ================= POLICY LAYER =================
        float keepProb = policy->computeKeepProbability(globalScore, idx);
        float compressionPressure = policy->computeCompressionStrength(globalScore);
        float qualityFactor = 1.0f - compressionPressure;

        (void)qualityFactor; // reserved for future encoder steering

        // ================= FINAL DECISION =================
        bool rawDecision =
            subjectActive ||
            eventActive ||
            (keepProb > (0.65f * (2.0f - bitrateBudget)));;

        // ---------------- MOMENTUM FILTER ----------------
        decisionMomentum =
            0.85f * decisionMomentum +
            0.15f * (rawDecision ? 1.0f : 0.0f);

        // hysteresis smoothing (prevents flicker)
        bool keepFrame =
            (decisionMomentum > 0.55f) ||
            (lastKeepDecision && decisionMomentum > 0.45f);

        lastKeepDecision = keepFrame;

        if (keepFrame)
        {
            bitrateBudget -= 0.03f;
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