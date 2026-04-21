#include "CompressionPolicy.hpp"
#include <algorithm>
#include <cmath>

// ---------------- KEEP PROBABILITY ----------------
// stable nonlinear mapping 
float CompressionPolicy::computeKeepProbability(float importance, uint64_t)
{
    importance = std::clamp(importance, 0.0f, 1.0f);

    // mild sharpening (prevents flat response)
    float p = std::tanh(2.5f * importance);

    return std::clamp(p, 0.0f, 1.0f);
}

// ----------------  DYNAMIC CRF ----------------
int CompressionPolicy::computeDynamicCRF(float importance, int baseCRF)
{
    importance = std::clamp(importance, 0.0f, 1.0f);

    // map importance → quality shift
    // high importance = lower CRF (better quality)
    // low importance  = higher CRF (more compression)

    float qualityBias = (1.0f - importance) * 10.0f; // range ~0–10

    int crf = static_cast<int>(baseCRF + qualityBias);

    return std::clamp(crf, 16, 35);
}

// ---------------- COMPRESSION STRENGTH ----------------
float CompressionPolicy::computeCompressionStrength(float importance)
{
    importance = std::clamp(importance, 0.0f, 1.0f);

    return 1.0f - importance;
}