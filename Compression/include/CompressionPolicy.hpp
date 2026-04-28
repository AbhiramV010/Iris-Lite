#pragma once

class CompressionPolicy
{
public:
    float motion(float v) const;
    float spatial(float v) const;
    float face(bool detected) const;
    float region(float v) const;

    float importanceScore(float motion,
        float spatial,
        float temporal) const;

    int computeCRF(float importance) const;
    int computeFPS(float importance) const;

private:
    float clamp01(float v) const;
};