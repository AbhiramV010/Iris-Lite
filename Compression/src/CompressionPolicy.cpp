#include "CompressionPolicy.hpp"
#include <algorithm>
#include <cmath>
#include "logging.hpp"

float CompressionPolicy::clamp01(float v)
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
    return detected ? 0.30f : 0.0f;
}

float CompressionPolicy::region(float v) const
{
    return clamp01(v) * 0.35f;
}

float CompressionPolicy::perceptualScore(float m, float s, float f, float r) const
{
    float raw = 0.45f * m + 0.25f * s + 0.15f * f + 0.15f * r;

    float score = std::log1p(raw * 6.0f) / std::log1p(6.0f);

    return clamp01(score);
}

int CompressionPolicy::computeCRF(float score, int baseCRF) const
{
    int shift = static_cast<int>((1.0f - score) * 14.0f);
    return std::clamp(baseCRF + shift, 18, 35);
}

int CompressionPolicy::computeFPS(float score, int baseFPS) const
{
    if (score > 0.75f) return baseFPS;
    if (score > 0.45f) return baseFPS - 6;
    return baseFPS - 10;
}