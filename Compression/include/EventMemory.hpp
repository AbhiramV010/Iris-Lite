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

static constexpr const char* SHM_DATA = "/iris_frame_buffer_data";
static constexpr const char* SHM_SIZES = "/iris_frame_sizes";
static constexpr const char* SHM_HEAD = "/iris_frame_head_tail";
};