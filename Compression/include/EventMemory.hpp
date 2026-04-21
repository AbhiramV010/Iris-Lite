#pragma once

#include <vector>
#include <cstdint>

struct EventRecord
{
    uint64_t startFrame;
    uint64_t endFrame;

    float avgImportance;
    float avgMotion;
    float avgEdges;
    float avgFaces;

    int crfUsed;
};

class EventMemory
{
public:
    EventMemory();

    void record(const EventRecord& event);

    float getAdaptiveBias() const;

    size_t size() const;

private:
    std::vector<EventRecord> history;

    static constexpr size_t MAX_HISTORY = 200;
};