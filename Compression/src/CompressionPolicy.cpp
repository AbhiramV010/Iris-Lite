#include "CompressionPolicy.hpp"
#include <cmath>
#include <algorithm>

float CompressionPolicy::clamp01(float v)
{
    return std::max(0.0f, std::min(1.0f, v));
}

float CompressionPolicy::motion(float v)
{
    return clamp01(v);
}

float CompressionPolicy::spatial(float v)
{
    return clamp01(v);
}

float CompressionPolicy::face(bool detected)
{
    return detected ? 0.30f : 0.0f;
}

float CompressionPolicy::region(float v)
{
    return clamp01(v) * 0.35f;
}

float CompressionPolicy::perceptualScore(float motion,
    float spatial,
    float face,
    float region)
{
    float raw =
        0.45f * motion +
        0.25f * spatial +
        0.15f * face +
        0.15f * region;

    // human perception curve (Weber-Fechner style)
    return std::log1p(raw * 6.0f) / std::log1p(6.0f);
}

int CompressionPolicy::computeCRF(float score, int baseCRF)
{
    int shift = (int)((1.0f - score) * 14.0f);
    return std::clamp(baseCRF + shift, 18, 35);
}

int CompressionPolicy::computeFPS(float score, int baseFPS)
{
    if (score > 0.75f) return baseFPS;
    if (score > 0.45f) return baseFPS - 6;
    return baseFPS - 10;
}