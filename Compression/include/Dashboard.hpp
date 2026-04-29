#pragma once

#include <string>
#include <mutex>

struct DashboardState
{
    std::string trigger = "idle";

    uint64_t startFrame = 0;
    uint64_t currentFrame = 0;
    uint64_t endFrame = 0;

    float pressure = 0.0f;
    float importance = 0.0f;

    int crf = 0;
    int fps = 0;

    float cpu = 0.0f;
    float thermal = 0.0f;

    float dropRate = 0.0f;

    bool encoderActive = false;
};

class Dashboard
{
public:
    void update(const DashboardState& s);

private:
    void render(const DashboardState& s);

private:
    std::mutex mtx;
};