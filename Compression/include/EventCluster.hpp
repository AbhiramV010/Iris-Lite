#pragma once

#include <vector>
#include <cstdint>
#include "EventTypes.hpp"

class EventCluster
{
public:
    void add(const EventWindow& e);
    std::vector<EventWindow> flush();

    bool shouldFlush() const;

private:
    bool canMerge(const EventWindow& a, const EventWindow& b) const;
    EventWindow merge(const EventWindow& a, const EventWindow& b) const;

    float adaptiveThreshold() const;

private:
    std::vector<EventWindow> buffer;

    uint64_t eventCountWindow = 0;
    uint64_t lastEventTime = 0;
};