#include "CompressionPolicy.hpp"
#include <algorithm>
#include <cmath>

float CompressionPolicy::clamp01(float v) const
{
    return std::max(0.0f, std::min(1.0f, v));
}

float CompressionPolicy::motion(float v) const
{
    return clamp01(v);
}

float CompressionPolicy::spatial(float v) const
{
    return clamp01(v);
}

float CompressionPolicy::face(bool detected) const
{
    return detected ? 0.4f : 0.0f;
}

float CompressionPolicy::region(float v) const
{
    return clamp01(v) * 0.3f;
}

float CompressionPolicy::importanceScore(float m,
    float s,
    float t) const
{
    float raw = 0.55f * m + 0.35f * s + 0.10f * t;

    // perceptual compression curve
    return std::log1p(raw * 6.0f) / std::log1p(6.0f);
}

int CompressionPolicy::computeCRF(float importance) const
{
    int shift = static_cast<int>((1.0f - importance) * 12.0f);
    return std::clamp(22 + shift, 18, 35);
}

int CompressionPolicy::computeFPS(float importance) const
{
    if (importance > 0.75f) return 24;
    if (importance > 0.45f) return 15;
    return 8;
}