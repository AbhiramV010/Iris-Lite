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

    // ---------------- DROP ----------------
    d.dropFrame = shouldDrop(d.importance);

    // ---------------- QUALITY (CPU-INDENT) ----------------
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

// ---------------- DROP POLICY ----------------

bool CompressionOrchestrator::shouldDrop(float importance)
{
    // ultra-conservative drop
    return importance < 0.10f;
}