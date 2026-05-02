#include "CompressionOrchestrator.hpp"
#include <algorithm>

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

    // ---------------- DROP (WITH TEMPORAL PROTECTION) ----------------
    static float lastImportance = 0.0f;

    bool baseDrop = policy->shouldSkipFrame(d.importance, pressure);

    // Protect continuity after important frames
    bool protect =
        (lastImportance > 0.6f) && (d.importance > 0.2f);

    d.dropFrame = baseDrop && !protect;

    lastImportance = d.importance;

    // ---------------- QUALITY ----------------
    d.crf = policy->computeCRF(d.importance);

    // informational only
    d.fps = policy->computeFPS(d.importance);

    return d;
}

// ---------------- FUSION ----------------

float CompressionOrchestrator::fuse(
    float engineScore,
    float memoryBias)
{
    float s = engineScore + memoryBias;
    return std::clamp(s, 0.0f, 1.0f);
}