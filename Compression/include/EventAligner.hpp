#pragma once

#include <cstdint>
#include <algorithm>

class EventAligner
{
public:
    EventAligner();

    struct AlignedEvent
    {
        uint64_t startFrame;
        uint64_t endFrame;
        uint64_t prePadding;
        uint64_t postPadding;
    };

    AlignedEvent align(
        uint64_t detectedStart,
        uint64_t detectedEnd,
        uint64_t bufferSize,
        float systemLatencyEstimate
    );

private:
    float avgLatency = 3.0f;
};