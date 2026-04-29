#include "SystemGovernor.hpp"
#include <fstream>
#include <algorithm>
#include <cmath>

// ---------------------------
// CPU LOAD
// ---------------------------

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

    if (totalDiff <= 0) return 0.0f;

    return 1.0f - (float)idleDiff / totalDiff;
}

// ---------------------------
// TEMPERATURE
// ---------------------------

float SystemGovernor::getThermalLoad()
{
    std::ifstream file("/sys/class/thermal/thermal_zone0/temp");

    float temp = 0;
    file >> temp;

    // normalize (85°C → 1.0)
    return std::min(1.0f, temp / 85000.0f);
}

// ---------------------------
// PRESSURE (WITH SMOOTHING)
// ---------------------------

float SystemGovernor::computePressure()
{
    float cpu = getCpuLoad();
    float temp = getThermalLoad();

    float raw =
        0.7f * cpu +
        0.3f * temp;

    static float smoothed = 0.0f;

    // EMA smoothing → prevents oscillation
    smoothed = 0.85f * smoothed + 0.15f * raw;

    return std::clamp(smoothed, 0.0f, 1.0f);
}

// ---------------------------
// ADAPTIVE CRF
// ---------------------------

int SystemGovernor::adaptiveCRF(int baseCRF)
{
    float p = computePressure();

    int shift = static_cast<int>(p * 12.0f);

    return std::clamp(baseCRF + shift, 18, 40);
}

// ---------------------------
// FRAME SKIP LOGIC
// ---------------------------

bool SystemGovernor::shouldSkipFrame(float importance)
{
    float p = computePressure();

    // only skip under real stress
    if (p < 0.65f)
        return false;

    // progressive skip curve
    float threshold =
        0.25f + (p - 0.65f) * 0.5f;

    return importance < threshold;
}