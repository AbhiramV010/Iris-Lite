#include "SystemGovernor.hpp"
#include <fstream>
#include <algorithm>
#include <chrono>

// ---------------- CPU LOAD ----------------
// Lightweight /proc approximation

float SystemGovernor::getCpuLoad()
{
    static long prevIdle = 0, prevTotal = 0;

    std::ifstream file("/proc/stat");
    std::string cpu;
    long user, nice, system, idle;

    file >> cpu >> user >> nice >> system >> idle;

    long idleTime = idle;
    long total = user + nice + system + idle;

    long diffIdle = idleTime - prevIdle;
    long diffTotal = total - prevTotal;

    prevIdle = idleTime;
    prevTotal = total;

    if (diffTotal == 0) return 0.0f;

    return 1.0f - (float)diffIdle / diffTotal;
}

// ---------------- THERMAL LOAD ----------------

float SystemGovernor::getThermalLoad()
{
    std::ifstream file("/sys/class/thermal/thermal_zone0/temp");

    float temp = 0.0f;
    file >> temp;

    // normalize (~100°C max assumption)
    return std::clamp(temp / 100000.0f, 0.0f, 1.0f);
}

// ---------------- PRESSURE MODEL ----------------

float SystemGovernor::computePressure()
{
    float cpu = getCpuLoad();
    float thermal = getThermalLoad();

    // weighted fusion
    float pressure = 0.7f * cpu + 0.3f * thermal;

    return std::clamp(pressure, 0.0f, 1.0f);
}
