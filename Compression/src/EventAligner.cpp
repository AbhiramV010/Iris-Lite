#include "EventAligner.hpp"
#include "logging.hpp"
#include <chrono>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <string>

EventAligner::EventAligner()
    : newEventReceived(false)
{
    auto now = std::chrono::system_clock::now();
    estimatedSystemTime = std::chrono::system_clock::to_time_t(now);
    logInfo("EventAligner initialized");
}

void EventAligner::parseEventFromIPC(const std::string& data)
{
    std::istringstream iss(data);
    std::string startTime, endTime, trigger;
    float duration;
    int motionSensor, doorSensor;

    if (iss >> startTime >> endTime >> std::quoted(trigger) >> duration >> motionSensor >> doorSensor)
    {
        lastEvent.startTimeStr = startTime;
        lastEvent.endTimeStr = endTime;
        lastEvent.triggerType = trigger;
        lastEvent.duration = duration;
        lastEvent.isMotionSensor = (motionSensor != 0);
        lastEvent.isDoorSensor = (doorSensor != 0);

        newEventReceived = true;

        logInfo("Event received: " + trigger + " (" + startTime + " - " + endTime + ")");
    }
}

uint64_t EventAligner::mapTimeToFrameIndex(const std::string& timeStr)
{
    double timeSeconds = parseTimeString(timeStr);
    return estimateFrameIndexFromTimestamp(timeSeconds);
}

uint64_t EventAligner::estimateFrameIndexFromTimestamp(double absoluteSeconds)
{
    uint64_t frameIndex = static_cast<uint64_t>(absoluteSeconds * 30);
    return frameIndex;
}

double EventAligner::parseTimeString(const std::string& ts)
{
    int hours = 0, minutes = 0, seconds = 0;
    sscanf(ts.c_str(), "%d:%d:%d", &hours, &minutes, &seconds);

    return hours * 3600.0 + minutes * 60.0 + seconds;
}