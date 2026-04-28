#pragma once
#include <cstdint>
#include <string>

struct EventWindow
{
    uint64_t startFrame;
    uint64_t endFrame;
    std::string trigger;
};