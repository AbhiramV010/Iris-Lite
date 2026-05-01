#pragma once
#include <cstdint>

class SharedFrameBuffer
{
public:
    bool initialize();
    bool isValid() const;

    uint8_t* getFrameBufferBase();
    uint32_t* getFrameSizes();
    uint64_t* getHeadTail();

private:
    bool mapMemory();

private:
    int fd = -1;

    int fd_sizes = -1;
    int fd_head = -1;

    uint8_t* frame_buffer = nullptr;
    uint32_t* frame_sizes = nullptr;
    uint64_t* head_tail = nullptr;
};
