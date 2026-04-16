#pragma once

#include <string>
#include <vector>
#include <cstdint>

struct DetectionEvent
{
    std::string startTimeStr;
    std::string endTimeStr;
    std::string triggerType;
    float duration;
    bool isMotionSensor;
    bool isDoorSensor;
};

class EventAligner
{
public:
    EventAligner();

    void parseEventFromIPC(const std::string& data);

    DetectionEvent& getLastEvent() { return lastEvent; }

    bool hasNewEvent() const { return newEventReceived; }

    void clearEventFlag() { newEventReceived = false; }

    uint64_t mapTimeToFrameIndex(const std::string& timeStr);

    uint64_t estimateFrameIndexFromTimestamp(double absoluteSeconds);

private:
    DetectionEvent lastEvent;
    bool newEventReceived;
    uint64_t captureStartTime;
    uint64_t estimatedSystemTime;

    double parseTimeString(const std::string& ts);
};