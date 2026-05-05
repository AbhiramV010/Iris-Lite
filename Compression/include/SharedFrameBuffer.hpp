#pragma once

#include <cstdint>
#include <vector>

constexpr int SLOT_SIZE = 200000;
constexpr int BUFFER_SIZE = 7200;

struct FrameSlot
{
    uint32_t size;
    uint64_t timestamp;
    uint64_t frame_id;
    uint8_t data[SLOT_SIZE];
};

class SharedFrameBuffer
{
public:
    bool initialize();
    bool isValid() const;

    const FrameSlot* getSlot(uint64_t index) const;

private:
    bool mapMemory();

    int fd = -1;
    FrameSlot* buffer = nullptr;
};