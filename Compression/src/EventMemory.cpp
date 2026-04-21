#include "EventMemory.hpp"
#include <algorithm>

EventMemory::EventMemory() {}

void EventMemory::record(const EventRecord& event)
{
    history.push_back(event);

    if (history.size() > MAX_HISTORY)
        history.erase(history.begin());
}

float EventMemory::getAdaptiveBias() const
{
    if (history.empty())
        return 0.0f;

    float avgImportance = 0.0f;

    for (const auto& e : history)
        avgImportance += e.avgImportance;

    avgImportance /= history.size();

    // low importance history → more aggressive compression allowed
    return 1.0f - avgImportance;
}

size_t EventMemory::size() const
{
    return history.size();
}