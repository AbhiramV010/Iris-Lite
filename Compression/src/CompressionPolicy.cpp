#include "CompressionPolicy.hpp"
#include <algorithm>
#include <cmath>

// ---------------- FRAME DECISION ----------------
// combines importance + system stability (momentum)
bool CompressionPolicy::shouldKeepFrame(float importance,
    float momentum,
    uint64_t frameIndex)
{
    importance = std::clamp(importance, 0.0f, 1.0f);
    momentum = std::clamp(momentum, 0.0f, 1.0f);

    // stability-aware importance (prevents flicker drops)
    float stability = 0.6f * importance + 0.4f * momentum;

    // nonlinear sharpening
    float boosted = stability * stability;

    // temporal adaptation (slight relaxation over time)
    float temporalBias = computeTemporalBias(frameIndex);

    float threshold = 0.15f - temporalBias;

    return boosted > threshold;
}

// ---------------- CRF CONTROL ----------------
// blends importance + momentum for stable compression quality
int CompressionPolicy::computeCRF(float importance,
    float momentum,
    int baseCRF)
{
    importance = std::clamp(importance, 0.0f, 1.0f);
    momentum = std::clamp(momentum, 0.0f, 1.0f);

    float stability = 0.5f * importance + 0.5f * momentum;

    float compressionFactor = std::pow(1.0f - stability, 2.0f);

    int crfShift = static_cast<int>(compressionFactor * 15.0f);

    int crf = baseCRF + crfShift;

    return std::clamp(crf, 16, 36);
}

// ---------------- COMPRESSION STRENGTH ----------------
float CompressionPolicy::computeCompressionStrength(float importance)
{
    importance = std::clamp(importance, 0.0f, 1.0f);

    return std::pow(1.0f - importance, 1.5f);
}

// ---------------- TEMPORAL BIAS ----------------
// NOT event detection — just stabilizes early/late frame behavior
float CompressionPolicy::computeTemporalBias(uint64_t frameIndex)
{
    // early frames = slightly more conservative compression
    float bias = std::exp(-frameIndex * 0.00001f);

    return std::clamp(bias * 0.1f, 0.0f, 0.1f);
}
