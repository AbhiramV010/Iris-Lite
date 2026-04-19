#pragma once

struct ImportanceSignal
{
    float motion = 0.0f;
    float edges = 0.0f;
    float faces = 0.0f;

    float global = 0.0f;

    // stability control (IMPORTANT for Pi performance later)
    float confidence = 1.0f;
};