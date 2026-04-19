#pragma once
#include <cstdint>

class CompressionPolicy
{
public:
    float computeKeepProbability(float globalImportance,
        uint64_t frameIndex);

    float computeCompressionStrength(float globalImportance);
};