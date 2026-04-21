#pragma once

#include <vector>
#include <cstdint>
#include "SharedFrameBuffer.hpp"

class SnapshotExtractor
{
public:
    static std::vector<std::vector<uint8_t>>
        extract(SharedFrameBuffer& buffer,
            uint64_t start,
            uint64_t end);
};