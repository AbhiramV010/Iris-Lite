#pragma once
#include <cstdint>

class SharedFrameBuffer
{
public:
    bool initialize();
    bool isValid() const;

    uint8_t* getFrameBufferBase();

private:
    bool mapMemory();

private:
    int fd = -1;
    uint8_t* frame_buffer = nullptr;
};