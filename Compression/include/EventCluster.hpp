#pragma once
#include "CompressionEngine.hpp"
#include <vector>

class EventCluster
{
public:
    void add(const EventWindow& e);
    bool shouldFlush();
    EventWindow merge();

private:
    std::vector<EventWindow> buffer;
};