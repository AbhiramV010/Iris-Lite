#pragma once
#include <cstdint>
#include <string>

struct EventWindow
{
    uint64_t startFrame = 0;
    uint64_t endFrame = 0;
    std::string trigger;

    float importance = 0.0f;
    float confidence = 0.0f;
};