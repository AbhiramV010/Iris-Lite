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
    return detected ? 0.25f : 0.0f;
}

float CompressionPolicy::region(float v) const
{
    return clamp01(v) * 0.25f;
}

float CompressionPolicy::importanceScore(float m,
                                        float s,
                                        float sensorBoost) const
{
    float raw = 0.55f * m + 0.35f * s + 0.10f * sensorBoost;

    // perceptual compression curve (important for competition)
    float score = std::log1p(raw * 5.0f) / std::log1p(5.0f);

    return clamp01(score);
}

int CompressionPolicy::computeCRF(float importance) const
{
    // IMPORTANT: higher importance → lower CRF (better quality)
    int base = 28;
    int shift = static_cast<int>((1.0f - importance) * 12.0f);

    return std::clamp(base + shift, 18, 35);
}
