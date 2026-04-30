#include "SystemGovernor.hpp"
#include <fstream>
#include <algorithm>
#include <cmath>

// ---------------- CPU LOAD ----------------

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

// ---------------- THERMAL LOAD ----------------

float SystemGovernor::getThermalLoad()
{
    std::ifstream file("/sys/class/thermal/thermal_zone0/temp");

    float temp = 0;
    file >> temp;

    // normalize (85°C -> 1.0)
    return std::min(1.0f, temp / 85000.0f);
}

// ---------------- PRESSURE SIGNAL ----------------
// PURE METRIC ONLY — NO CONTROL ROLE

float SystemGovernor::computePressure()
{
    float cpu = getCpuLoad();
    float temp = getThermalLoad();

    float raw = 0.7f * cpu + 0.3f * temp;

    static float ema = 0.0f;
    ema = 0.85f * ema + 0.15f * raw;

    return std::clamp(ema, 0.0f, 1.0f);
}