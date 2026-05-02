#include "CompressionPolicy.hpp"
#include <algorithm>

float CompressionPolicy::clamp01(float v) const
{
    return std::max(0.0f, std::min(1.0f, v));
}

// --------------------------------------------------
// FUSION
// --------------------------------------------------

float CompressionPolicy::fuseImportance(float engineScore,
    float faceBoost,
    float systemPressure) const
{
    float s = clamp01(engineScore);

    // face boost remains additive
    s = std::min(1.0f, s + faceBoost);

    // softer pressure penalty (prevents over-degradation)
    s *= (1.0f - 0.25f * clamp01(systemPressure));

    return clamp01(s);
}

// --------------------------------------------------
// CRF TIERS (STABLE)
// --------------------------------------------------

int CompressionPolicy::computeCRF(float importance) const
{
    importance = clamp01(importance);

    if (importance > 0.75f) return 20;
    if (importance > 0.50f) return 24;
    if (importance > 0.30f) return 28;
    return 32;
}

// --------------------------------------------------
// FPS (INFO ONLY)
// --------------------------------------------------

int CompressionPolicy::computeFPS(float importance) const
{
    if (importance > 0.80f) return 24;
    if (importance > 0.55f) return 18;
    if (importance > 0.30f) return 12;
    return 8;
}

// --------------------------------------------------
// PRESSURE-AWARE FRAME SKIP
// --------------------------------------------------

bool CompressionPolicy::shouldSkipFrame(float importance,
    float pressure) const
{
    pressure = clamp01(pressure);

    // Normal operation: keep everything
    if (pressure < 0.75f)
        return false;

    // Moderate pressure: drop only low-value frames
    if (pressure < 0.9f)
        return importance < 0.3f;

    // Critical pressure: aggressive drop
    return importance < 0.5f;
}