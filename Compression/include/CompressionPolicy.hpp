#pragma once

class CompressionPolicy
{
public:
    // Pure decision mapping ONLY

    float fuseImportance(float engineScore,
        float faceBoost,
        float systemPressure) const;

    int computeCRF(float importance) const;
    int computeFPS(float importance) const;

    bool shouldSkipFrame(float importance,
        float systemPressure) const;

private:
    float clamp01(float v) const;
};