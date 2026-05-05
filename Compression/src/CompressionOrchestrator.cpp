#include "CompressionOrchestrator.hpp"
#include <algorithm>
#include <cmath>

// ---------------- CONSTRUCTOR ----------------

CompressionOrchestrator::CompressionOrchestrator(
    const Config& cfg_,
    ImportanceEngine* imp,
    SystemGovernor* gov,
    CompressionPolicy* pol,
    ImportanceMemory* mem)
    : cfg(cfg_), importance(imp), governor(gov), policy(pol), memory(mem)
{
}

// ---------------- MAIN PIPELINE ----------------

CompressionOrchestrator::Decision CompressionOrchestrator::compute(
    const cv::Mat& frame,
    const cv::Mat& prev,
    uint64_t frameIndex,
    uint64_t peakFrame,
    const std::string& trigger)
{
    Decision d{};

    if (!importance || !policy)
        return d;

    // ---------------- PERCEPTION ----------------
    auto sig = importance->analyze(frame, prev, frameIndex, peakFrame);
    float engineScore = sig.score;

    // ---------------- MEMORY ----------------
    float bias = 0.0f;
    if (memory)
        bias = memory->getBias(trigger);

    // ---------------- FUSION ----------------
    d.importance = fuse(engineScore, bias);

    // ---------------- PRESSURE ----------------
    float pressure = governor ? governor->computePressure() : 0.0f;

    // ---------------- TEMPORAL CONTINUITY ----------------
    static float lastImportance = 0.0f;
    static int continuityFrames = 0;

    bool highImportance = d.importance > 0.6f;
    bool mediumImportance = d.importance > 0.3f;

    if (highImportance)
        continuityFrames = 6; // protect next ~6 frames
    else if (continuityFrames > 0)
        continuityFrames--;

    // ---------------- DROP LOGIC (SMART, NOT AGGRESSIVE) ----------------
    bool baseDrop = policy->shouldSkipFrame(d.importance, pressure);

    bool protect =
        (continuityFrames > 0) ||                     // after important frame
        (lastImportance > 0.5f && mediumImportance);  // smooth transitions

    d.dropFrame = baseDrop && !protect;

    lastImportance = d.importance;

    // ---------------- QUALITY (PRESSURE-AWARE) ----------------
    float crfBase = policy->computeCRF(d.importance);

    // degrade slightly under pressure instead of dropping frames
    d.crf = std::clamp(
        crfBase + (pressure * 6.0f),
        18.0f,
        40.0f
    );

    // ---------------- FPS ADAPTATION ----------------
    d.fps = policy->computeFPS(d.importance);

    return d;
}

// ---------------- FUSION ----------------

float CompressionOrchestrator::fuse(
    float engineScore,
    float memoryBias)
{
    // non-linear fusion (memory matters more at mid-level importance)
    float fused = engineScore + (memoryBias * (0.5f + engineScore));

    return std::clamp(fused, 0.0f, 1.0f);
}