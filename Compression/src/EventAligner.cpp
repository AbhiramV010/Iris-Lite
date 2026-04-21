#include "EventAligner.hpp"
#include <algorithm>

// ---------------- CONSTRUCTOR ----------------

EventAligner::EventAligner()
    : avgLatency(3.0f)
{
}

// ---------------- ALIGN EVENT WINDOW ----------------

EventAligner::AlignedEvent EventAligner::align(
    uint64_t detectedStart,
    uint64_t detectedEnd,
    uint64_t bufferSize,
    float systemLatencyEstimate
)
{
    AlignedEvent out{};

    // ---------------- EDGE CASE SAFETY ----------------
    if (bufferSize == 0)
        return out;

    // ensure valid ordering
    if (detectedEnd < detectedStart)
        std::swap(detectedStart, detectedEnd);

    // ---------------- LATENCY MODEL UPDATE ----------------
    // smooth adaptive estimator (Pi-safe, no ML overhead)
    avgLatency =
        0.9f * avgLatency +
        0.1f * systemLatencyEstimate;

    // ---------------- PRE-PADDING ----------------
    float estimatedPadding = avgLatency + systemLatencyEstimate;

    uint64_t prePadding = static_cast<uint64_t>(
        std::clamp(estimatedPadding, 2.0f, 15.0f)
        );

    // ---------------- POST-PADDING ----------------
    uint64_t postPadding = prePadding / 2;

    // ---------------- WINDOW EXPANSION ----------------
    int64_t start = static_cast<int64_t>(detectedStart) - static_cast<int64_t>(prePadding);
    int64_t end = static_cast<int64_t>(detectedEnd) + static_cast<int64_t>(postPadding);

    // ---------------- BOUND CLAMPING ----------------
    if (start < 0)
        start = 0;

    if (end < 0)
        end = 0;

    if (static_cast<uint64_t>(end) >= bufferSize)
        end = bufferSize - 1;

    if (static_cast<uint64_t>(start) >= bufferSize)
        start = bufferSize - 1;

    // final guarantee
    if (end < start)
        end = start;

    // ---------------- OUTPUT ----------------
    out.startFrame = static_cast<uint64_t>(start);
    out.endFrame = static_cast<uint64_t>(end);
    out.prePadding = prePadding;
    out.postPadding = postPadding;

    return out;
}