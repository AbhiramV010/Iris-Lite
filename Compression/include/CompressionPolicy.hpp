#pragma once
#include <cstdint>

class CompressionPolicy
{
public:
    // Probability of keeping a frame
    float computeKeepProbability(float importance, uint64_t frameIndex);

    // NEW: dynamic CRF adjustment (core upgrade)
    int computeDynamicCRF(float importance, int baseCRF);

    // Optional: compression pressure signal (kept for explainability)
    float computeCompressionStrength(float importance);
};