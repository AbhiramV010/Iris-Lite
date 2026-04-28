#include "CompressionPolicy.hpp"
#include <algorithm>
#include <cmath>

float CompressionPolicy::clamp01(float v) const
{
    return std::max(0.0f, std::min(1.0f, v));
}

float CompressionPolicy::motion(float v) const
{
    // motion is the strongest perceptual signal
    return clamp01(std::pow(v, 0.85f)); // slight boost
}

float CompressionPolicy::spatial(float v) const
{
    // edges matter but less than motion
    return clamp01(std::pow(v, 1.1f)); // slight suppression
}

float CompressionPolicy::face(bool detected) const
{
    return detected ? 0.45f : 0.0f;
}

float CompressionPolicy::region(float v) const
{
    return clamp01(v) * 0.3f;
}

float CompressionPolicy::importanceScore(float m,
    float s,
    float sensorBoost) const
{
    // Motion dominates perception
    float raw =
        0.65f * m +
        0.25f * s +
        0.10f * sensorBoost;

    // Sharper perceptual curve
    float score = std::log1p(raw * 8.0f) / std::log1p(8.0f);

    return clamp01(score);
}

int CompressionPolicy::computeCRF(float importance) const
{
    // wider dynamic range → better compression gains
    int shift = static_cast<int>((1.0f - importance) * 16.0f);

    // important = 18–22, background = up to 36
    return std::clamp(20 + shift, 18, 36);
}

int CompressionPolicy::computeFPS(float importance) const
{
    // DO NOT aggressively drop FPS (you learned this the hard way)
    if (importance > 0.8f) return 24;
    if (importance > 0.5f) return 20;
    return 15;
}