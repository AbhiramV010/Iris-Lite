#pragma once

class CompressionPolicy
{
public:
    float motion(float v) const;
    float spatial(float v) const;
    float face(bool detected) const;
    float region(float v) const;

    // unified perceptual model
    float importanceScore(float motion,
                          float spatial,
                          float sensorBoost) const;

    int computeCRF(float importance) const;

private:
    float clamp01(float v) const;
};
