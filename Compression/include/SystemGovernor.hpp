#pragma once

class SystemGovernor
{
public:
    float getCpuLoad();
    float getThermalLoad();
    float computePressure();
};