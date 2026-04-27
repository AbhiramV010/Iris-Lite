#include "SystemGovernor.hpp"
#include <fstream>
#include <algorithm>

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

    return std::min(1.0f, temp / 85000.0f);
}

float SystemGovernor::computePressure()
{
    return std::clamp(
        0.7f * getCpuLoad() +
        0.3f * getThermalLoad(),
        0.0f, 1.0f
    );
}