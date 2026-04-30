#include "CompressionPolicy.hpp"
#include <algorithm>
#include <cmath>

float CompressionPolicy::clamp01(float v) const
{
    return std::max(0.0f, std::min(1.0f, v));
}

// --------------------------------------------------
// SINGLE FUSION POINT 
// --------------------------------------------------

float CompressionPolicy::fuseImportance(float engineScore,
    float faceBoost,
    float systemPressure) const
{
    float s = clamp01(engineScore);

    // face is now PURE boost
    s = std::min(1.0f, s + faceBoost);

    // pressure reduces perceived importance slightly
    s *= (1.0f - 0.4f * clamp01(systemPressure));

    return clamp01(s);
}

// --------------------------------------------------
// ENCODING CONTROL
// --------------------------------------------------

int CompressionPolicy::computeCRF(float importance) const
{
    float inv = 1.0f - clamp01(importance);

    int shift = static_cast<int>(std::pow(inv, 1.4f) * 14.0f);

    return std::clamp(20 + shift, 18, 36);
}

int CompressionPolicy::computeFPS(float importance) const
{
    if (importance > 0.80f) return 24;
    if (importance > 0.55f) return 18;
    if (importance > 0.30f) return 12;
    return 8;
}

bool CompressionPolicy::shouldSkipFrame(float importance,
    float pressure) const
{
    if (pressure < 0.65f)
        return false;

    float threshold = 0.25f + (pressure - 0.65f) * 0.55f;

    return importance < threshold;
}