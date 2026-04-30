#pragma once

class SystemGovernor
{
public:
    float getCpuLoad();
    float getThermalLoad();

    // PURE OBSERVABILITY SIGNAL (0–1)
    float computePressure();
};