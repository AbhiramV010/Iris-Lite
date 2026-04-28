#pragma once

#include <vector>
#include "CompressionEngine.hpp"

class EventCluster
{
public:
    void add(const EventWindow& e);
    bool shouldFlush() const;
    std::vector<EventWindow> flush();

private:
    bool canMerge(const EventWindow& a, const EventWindow& b) const;
    EventWindow merge(const EventWindow& a, const EventWindow& b) const;
    float adaptiveThreshold() const;

private:
    std::vector<EventWindow> buffer;
    uint64_t lastEventTime = 0;
    size_t eventCountWindow = 0;
};