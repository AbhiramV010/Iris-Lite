#include "SnapshotExtractor.hpp"

std::vector<std::vector<uint8_t>>
SnapshotExtractor::extract(SharedFrameBuffer& buffer,
    uint64_t start,
    uint64_t end)
{
    std::vector<std::vector<uint8_t>> frames;

    if (!buffer.isValid())
        return frames;

    // consistency check
    uint64_t head1 = buffer.getHead();
    uint64_t tail = buffer.getTail();
    uint64_t head2 = buffer.getHead();

    if (head1 != head2)
        return {};

    uint64_t head = head1;

    // clamp
    if (start < tail)
        start = tail;

    if (end > head)
        end = head;

    if (start >= end)
        return frames;

    frames.reserve(static_cast<size_t>(end - start));

    for (uint64_t i = start; i < end; i++)
    {
        std::vector<uint8_t> jpeg;

        if (buffer.getFrame(i, jpeg))
            frames.emplace_back(std::move(jpeg));
    }

    return frames;
}