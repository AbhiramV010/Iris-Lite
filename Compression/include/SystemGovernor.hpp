#pragma once

class SystemGovernor
{
public:
    float getCpuLoad();
    float getThermalLoad();

    // FINAL OUTPUT: single control signal
    float computePressure();

    // NEW: helpers for encoder control
    int adaptiveCRF(int baseCRF);
    bool shouldSkipFrame(float importance);
};