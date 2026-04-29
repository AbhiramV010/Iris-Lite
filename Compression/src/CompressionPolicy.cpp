#include "CompressionPolicy.hpp"
#include <algorithm>
#include <cmath>

float CompressionPolicy::clamp01(float v) const
{
    return std::max(0.0f, std::min(1.0f, v));
}

// ---------------------------
// SIGNAL NORMALIZATION
// ---------------------------

float CompressionPolicy::motion(float v) const
{
    // motion is perceptually strong → slight boost
    return std::pow(clamp01(v), 0.8f);
}

float CompressionPolicy::spatial(float v) const
{
    // edges/details matter less than motion
    return std::pow(clamp01(v), 1.2f);
}

float CompressionPolicy::face(bool detected) const
{
    // reserved for future (face detection)
    return detected ? 0.4f : 0.0f;
}

float CompressionPolicy::region(float v) const
{
    return clamp01(v) * 0.3f;
}

// ---------------------------
// IMPORTANCE MODEL (CORE)
// ---------------------------

float CompressionPolicy::importanceScore(float m,
    float s,
    float t) const
{
    // weighted perceptual fusion
    float motionW = 0.60f * motion(m);
    float spatialW = 0.25f * spatial(s);
    float temporalW = 0.15f * t;

    float raw = motionW + spatialW + temporalW;

    // logarithmic compression (human perception curve)
    float perceptual = std::log1p(raw * 8.0f) / std::log1p(8.0f);

    return clamp01(perceptual);
}

// ---------------------------
// ENCODING CONTROL
// ---------------------------

int CompressionPolicy::computeCRF(float importance) const
{
    // sharper drop-off for low-importance frames
    float inv = 1.0f - importance;

    int shift = static_cast<int>(std::pow(inv, 1.5f) * 14.0f);

    return std::clamp(20 + shift, 18, 36);
}

int CompressionPolicy::computeFPS(float importance) const
{
    // smoother tiers (avoid abrupt jumps)
    if (importance > 0.80f) return 24;
    if (importance > 0.55f) return 18;
    if (importance > 0.30f) return 12;
    return 8;
}