#include "EventCluster.hpp"
#include "logging.hpp"
#include <algorithm>
#include <cmath>
#include <vector>

static constexpr uint64_t BASE_GAP = 120;

// ----------------------------
// Trigger importance model
// ----------------------------
static float getTriggerWeight(const std::string& t)
{
    if (t.find("door") != std::string::npos) return 1.0f;
    if (t.find("gunshot") != std::string::npos) return 1.0f;
    if (t.find("screaming") != std::string::npos) return 0.95f;
    if (t.find("glass") != std::string::npos) return 0.9f;

    if (t.find("motion") != std::string::npos) return 0.4f;
    if (t.find("grass") != std::string::npos) return 0.2f;

    return 0.5f;
}

float EventCluster::adaptiveThreshold() const
{
    float density = std::min(3.0f, eventCountWindow / 8.0f);
    return BASE_GAP * density;
}

// ----------------------------
// MERGE LOGIC (semantic-aware)
// ----------------------------
bool EventCluster::canMerge(const EventWindow& a, const EventWindow& b) const
{
    uint64_t gap = b.startFrame > a.endFrame ?
        (b.startFrame - a.endFrame) : 0;

    if (gap > adaptiveThreshold())
        return false;

    float ta = getTriggerWeight(a.trigger);
    float tb = getTriggerWeight(b.trigger);

    float similarity = 1.0f - std::abs(ta - tb);

    return similarity > 0.55f;
}

// ----------------------------
// MERGE OPERATION
// ----------------------------
EventWindow EventCluster::merge(const EventWindow& a, const EventWindow& b) const
{
    EventWindow out;

    out.startFrame = std::min(a.startFrame, b.startFrame);
    out.endFrame = std::max(a.endFrame, b.endFrame);

    out.trigger = a.trigger + "+" + b.trigger;

    return out;
}

// ----------------------------
// SCORING FUNCTION (core Phase 3A)
// ----------------------------
static float scoreEvent(const EventWindow& e, size_t clusterSize)
{
    float trigger = getTriggerWeight(e.trigger);
    float density = std::min(1.0f, clusterSize / 5.0f);

    float duration =
        std::min(1.0f, float(e.endFrame - e.startFrame) / 300.0f);

    float raw =
        0.65f * trigger +
        0.25f * density +
        0.10f * duration;

    return std::clamp(raw, 0.0f, 1.0f);
}

// ----------------------------
// ADD EVENT
// ----------------------------
void EventCluster::add(const EventWindow& e)
{
    eventCountWindow++;

    if (buffer.empty())
    {
        buffer.push_back(e);
        return;
    }

    EventWindow& last = buffer.back();

    if (canMerge(last, e))
        last = merge(last, e);
    else
        buffer.push_back(e);
}

// ----------------------------
// FLUSH = PRIORITY SORTED MEMORY CHUNK
// ----------------------------
std::vector<EventWindow> EventCluster::flush()
{
    struct Scored
    {
        EventWindow e;
        float score;
    };

    std::vector<Scored> scored;

    for (auto& e : buffer)
    {
        float s = scoreEvent(e, buffer.size());
        e.importance = s;
        scored.push_back({ e, s });
    }

    std::sort(scored.begin(), scored.end(),
        [](const Scored& a, const Scored& b)
        {
            return a.score > b.score;
        });

    std::vector<EventWindow> out;
    for (auto& s : scored)
        out.push_back(s.e);

    buffer.clear();
    eventCountWindow = 0;

    logInfo("EventCluster flushed (priority ranked)");

    return out;
}

// ----------------------------
bool EventCluster::shouldFlush() const
{
    return buffer.size() >= 3 || eventCountWindow >= 10;
}