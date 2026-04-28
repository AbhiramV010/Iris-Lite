#include "EventCluster.hpp"
#include "logging.hpp"
#include <algorithm>
#include "EventTypes.hpp"
#include "SharedFrameBuffer.hpp"
#include <vector>
#include <cstdint>
static constexpr uint64_t BASE_GAP = 120;

void EventCluster::add(const EventWindow& e)
{
    eventCountWindow++;

    if (buffer.empty())
    {
        buffer.push_back(e);
        lastEventTime = e.endFrame;
        return;
    }

    EventWindow& last = buffer.back();

    if (canMerge(last, e))
        last = merge(last, e);
    else
        buffer.push_back(e);

    lastEventTime = e.endFrame;
}

bool EventCluster::canMerge(const EventWindow& a, const EventWindow& b) const
{
    uint64_t threshold = adaptiveThreshold();

    if (b.startFrame <= a.endFrame)
        return true;

    return (b.startFrame - a.endFrame) <= threshold;
}

EventWindow EventCluster::merge(const EventWindow& a, const EventWindow& b) const
{
    return {
        std::min(a.startFrame, b.startFrame),
        std::max(a.endFrame, b.endFrame),
        a.trigger + "+" + b.trigger
    };
}

float EventCluster::adaptiveThreshold() const
{
    float density = std::min(3.0f, eventCountWindow / 8.0f);
    return BASE_GAP * density;
}

bool EventCluster::shouldFlush() const
{
    return buffer.size() >= 3 || eventCountWindow >= 10;
}

std::vector<EventWindow> EventCluster::flush()
{
    std::vector<EventWindow> out;
    out.swap(buffer);

    eventCountWindow = 0;

    logInfo("EventCluster flush: " + std::to_string(out.size()));
    return out;
}