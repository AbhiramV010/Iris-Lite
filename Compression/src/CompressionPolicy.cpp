#include "CompressionPolicy.hpp"
#include <algorithm>
#include <cmath>

float CompressionPolicy::computeKeepProbability(float importance, uint64_t)
{
    // prevent instability
    importance = std::clamp(importance, 0.0f, 1.0f);

    // nonlinear boost for high-importance frames
    float p = importance * importance;

    return std::clamp(p, 0.0f, 1.0f);
}

float CompressionPolicy::computeCompressionStrength(float globalImportance)
{
    globalImportance = std::clamp(globalImportance, 0.0f, 1.0f);

    // inverse relationship
    float strength = 1.0f - globalImportance;

    return std::clamp(strength, 0.1f, 1.0f);
}