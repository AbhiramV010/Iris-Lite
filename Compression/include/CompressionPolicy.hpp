#pragma once

class CompressionPolicy
{
public:
    float motion(float v);
    float spatial(float v);
    float face(bool detected);
    float region(float v);

    float perceptualScore(float motion,
        float spatial,
        float face,
        float region);

    int computeCRF(float score, int baseCRF);
    int computeFPS(float score, int baseFPS);

private:
    float clamp01(float v);
};