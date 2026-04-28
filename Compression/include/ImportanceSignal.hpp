#pragma once

#include <cstdint>

struct ImportanceSignal
{
    float motion = 0.0f;
    float spatial = 0.0f;
    float temporal = 0.0f;
    float score = 0.0f;
};