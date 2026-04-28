#include "CompressionPolicy.hpp"
#include <algorithm>
#include <cmath>

float CompressionPolicy::clamp01(float v) const
{
    return std::max(0.0f, std::min(1.0f, v));
}

float CompressionPolicy::motion(float v) const
{
    return clamp01(std::pow(v, 0.8f)); // amplify small motion
}

float CompressionPolicy::spatial(float v) const
{
    return clamp01(std::pow(v, 1.15f)); // suppress noise edges
}

float CompressionPolicy::region(float v) const
{
    return clamp01(v * 0.4f); // stronger center weighting
}

float CompressionPolicy::importanceScore(float m,
    float s,
    float r,
    float t) const
{
    // perceptual weighting tuned for human attention
    float raw =
        0.5f * m +
        0.25f * s +
        0.15f * r +
        0.10f * t;

    // non-linear perceptual curve
    float score = std::log1p(raw * 9.0f) / std::log1p(9.0f);

    return clamp01(score);
}

int CompressionPolicy::computeCRF(float importance) const
{
    int shift = static_cast<int>((1.0f - importance) * 18.0f);

    return std::clamp(20 + shift, 18, 38);
}