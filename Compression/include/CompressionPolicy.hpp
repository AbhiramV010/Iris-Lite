#pragma once

class CompressionPolicy
{
public:
    float motion(float v) const;
    float spatial(float v) const;
    float face(bool detected) const;
    float region(float v) const;

    float perceptualScore(float motion,
        float spatial,
        float face,
        float region) const;

    int computeCRF(float score, int baseCRF) const;
    int computeFPS(float score, int baseFPS) const;

private:
    static float clamp01(float v);
};