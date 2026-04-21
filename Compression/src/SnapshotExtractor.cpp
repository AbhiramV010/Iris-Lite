#include "SnapshotExtractor.hpp"

std::vector<std::vector<uint8_t>>
SnapshotExtractor::extract(SharedFrameBuffer& buffer,
                           uint64_t start,
                           uint64_t end)
{
    std::vector<std::vector<uint8_t>> frames;

    if (!buffer.isValid())
        return frames;

    uint64_t head = buffer.getHead();
    uint64_t tail = buffer.getTail();

    // Clamp range to valid buffer window
    uint64_t head1 = buffer.getHead();
    uint64_t tail = buffer.getTail();
    uint64_t head2 = buffer.getHead();

    // if head changed during read, retry
    if (head1 != head2)
        return {};

    uint64_t head = head1;

    for (uint64_t i = start; i < end; i++)
    {
        std::vector<uint8_t> jpeg;

        if (buffer.getFrame(i, jpeg))
        {
            frames.push_back(std::move(jpeg));
        }
    }

    return frames;
}