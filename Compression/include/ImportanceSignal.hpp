#pragma once

struct ImportanceSignal
{
    float motion = 0.0f;
    float edges = 0.0f;
    float faces = 0.0f;

    float global = 0.0f;
    float confidence = 1.0f;
};