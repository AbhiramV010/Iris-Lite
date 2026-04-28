#pragma once

#include <vector>
#include "EventTypes.hpp"

class EventCluster
{
public:
    void add(const EventWindow& e);

    std::vector<EventWindow> flush();

private:
    bool canMerge(const EventWindow& a, const EventWindow& b) const;
    EventWindow merge(const EventWindow& a, const EventWindow& b) const;

private:
    std::vector<EventWindow> buffer;
};