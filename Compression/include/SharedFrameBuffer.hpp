#pragma once
#include <cstdint>
#include <vector>

class SharedFrameBuffer
{
public:
    bool initialize();
    bool isValid() const;

    bool getFrame(uint64_t index, std::vector<uint8_t>& outJpeg);

private:
    bool mapMemory();

private:
    int fd_data = -1;
    int fd_sizes = -1;
    int fd_head_tail = -1;

    uint8_t* frame_buffer = nullptr;
    uint32_t* frame_sizes = nullptr;
    uint64_t* head_tail = nullptr;

    size_t bufferSize = 0;
    size_t slotSize = 0;
};