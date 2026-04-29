#pragma once
#include <cstdint>

class SharedFrameBuffer
{
public:
    bool initialize();
    bool isValid() const;

    // returns pointer directly to raw frame
    uint8_t* getFramePtr();

private:
    bool mapMemory();

private:
    int fd = -1;
    uint8_t* frame_buffer = nullptr;
};