#include "SystemGovernor.hpp"
#include <fstream>
#include <algorithm>
#include <cmath>

float SystemGovernor::getCpuLoad()
{
    std::ifstream file("/proc/stat");

    std::string cpu;
    long user, nice, system, idle;

    file >> cpu >> user >> nice >> system >> idle;

    static long prevIdle = 0;
    static long prevTotal = 0;

    long total = user + nice + system + idle;
    long idleDiff = idle - prevIdle;
    long totalDiff = total - prevTotal;

    prevIdle = idle;
    prevTotal = total;

    if (totalDiff == 0) return 0.0f;

    return 1.0f - (float)idleDiff / totalDiff;
}

float SystemGovernor::getThermalLoad()
{
    std::ifstream file("/sys/class/thermal/thermal_zone0/temp");
    float temp = 0;
    file >> temp;

    // normalize (85C = max pressure)
    return std::min(1.0f, temp / 85000.0f);
}

float SystemGovernor::computePressure()
{
    float cpu = getCpuLoad();
    float temp = getThermalLoad();

    // nonlinear stress curve (IMPORTANT)
    float pressure =
        0.65f * cpu +
        0.35f * temp;

    return std::clamp(pressure, 0.0f, 1.0f);
}

int SystemGovernor::adaptiveCRF(int baseCRF)
{
    float p = computePressure();

    // Pi-safe compression ramp
    int shift = static_cast<int>(p * 10.0f);

    return std::clamp(baseCRF + shift, 18, 40);
}

bool SystemGovernor::shouldSkipFrame(float importance)
{
    float p = computePressure();

    // only skip LOW importance when under pressure
    if (p < 0.6f) return false;

    return importance < 0.25f;
}