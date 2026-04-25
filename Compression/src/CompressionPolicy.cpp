#include "CompressionPolicy.hpp"
#include <algorithm>
#include <cmath>

// ---------------- FRAME DECISION ----------------
bool CompressionPolicy::shouldKeepFrame(float importance,
    float momentum,
    uint64_t frameIndex)
{
    importance = std::clamp(importance, 0.0f, 1.0f);
    momentum = std::clamp(momentum, 0.0f, 1.0f);

    // perceptual stability blend
    float stability = 0.65f * importance + 0.35f * momentum;

    // FIX: remove destructive squaring (was collapsing values too hard)
    float boosted = std::pow(stability, 0.8f);

    float temporalBias = computeTemporalBias(frameIndex);

    // safer baseline threshold
    float threshold = 0.07f - temporalBias;

    // HARD GUARANTEE: never lose too many frames
    // ensures temporal continuity (prevents 10s → 1s collapse)
    static uint64_t frameCounter = 0;
    frameCounter++;

    const int GUARANTEE_INTERVAL = 5; // keep at least 20% of frames

    if (frameCounter % GUARANTEE_INTERVAL == 0)
        return true;

    return boosted > threshold;
}

// ---------------- CRF CONTROL ----------------
int CompressionPolicy::computeCRF(float importance,
    float momentum,
    int baseCRF)
{
    importance = std::clamp(importance, 0.0f, 1.0f);
    momentum = std::clamp(momentum, 0.0f, 1.0f);

    float stability = 0.5f * importance + 0.5f * momentum;

    float compressionFactor = std::pow(1.0f - stability, 2.0f);

    int crfShift = static_cast<int>(compressionFactor * 12.0f);

    int crf = baseCRF + crfShift;

    return std::clamp(crf, 18, 34);
}

// ---------------- COMPRESSION STRENGTH ----------------
float CompressionPolicy::computeCompressionStrength(float importance)
{
    importance = std::clamp(importance, 0.0f, 1.0f);
    return std::pow(1.0f - importance, 1.25f);
}

// ---------------- TEMPORAL BIAS ----------------
float CompressionPolicy::computeTemporalBias(uint64_t frameIndex)
{
    float bias = std::exp(-frameIndex * 0.00001f);
    return std::clamp(bias * 0.08f, 0.0f, 0.08f);
}