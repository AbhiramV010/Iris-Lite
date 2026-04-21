#pragma once
#include <cstdint>

class CompressionPolicy
{
public:
    // Frame-level decision (keep or drop)
    bool shouldKeepFrame(float importance,
        float momentum,
        uint64_t frameIndex);

    // Segment-level quality control
    int computeCRF(float importance,
        float momentum,
        int baseCRF);

    // Compression intensity signal (debug/analysis)
    float computeCompressionStrength(float importance);

    // Soft transition detector (NOT event detection)
    // Used to bias compression smoothness, not trigger logic
    float computeTemporalBias(uint64_t frameIndex);
}; 