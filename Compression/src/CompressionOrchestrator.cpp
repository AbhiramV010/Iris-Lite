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

// ---------------- MAIN DECISION PIPELINE ----------------

CompressionOrchestrator::Decision CompressionOrchestrator::compute(
    const cv::Mat& frame,
    const cv::Mat& prev,
    uint64_t frameIndex,
    uint64_t peakFrame,
    const std::string& trigger)
{
    Decision d{};

    // 1. PERCEPTION
    auto sig = importance->analyze(frame, prev, frameIndex, peakFrame);
    float engineScore = sig.score;

    // 2. MEMORY BIAS (learned trigger correction)
    float bias = memory->getBias(trigger);

    // 3. PURE FUSION (NO SYSTEM CONTROL)
    d.importance = fuse(engineScore, bias);

    // 4. DROP LOGIC (PURE SEMANTIC THRESHOLD)
    d.dropFrame = shouldDrop(d.importance);

    // 5. CRF MAPPING (ONLY IMPORTANCE DRIVEN)
    d.crf = policy->computeCRF(d.importance);

    // 6. FPS is informational ONLY (not used downstream)
    d.fps = policy->computeFPS(d.importance);

    return d;
}

// ---------------- FUSION MODEL ----------------

float CompressionOrchestrator::fuse(
    float engineScore,
    float memoryBias)
{
    float s = engineScore + memoryBias;

    // keep bounded semantic correction only
    return std::clamp(s, 0.0f, 1.0f);
}

// ---------------- DROP POLICY ----------------

bool CompressionOrchestrator::shouldDrop(float importance)
{
    // hard semantic cutoff only
    return importance < 0.15f;
}