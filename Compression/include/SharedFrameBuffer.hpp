#pragma once

#include <cstdint>

constexpr int SLOT_SIZE = 200000;
constexpr int BUFFER_SIZE = 7200;

class SharedFrameBuffer
{
public:
    bool initialize();
    bool isValid() const;

    const uint8_t* getFrameData(uint64_t index) const;
    uint32_t getFrameSize(uint64_t index) const;

    const uint64_t* getHeadTail() const;

private:
    bool mapMemory();

private:
    int dataFd = -1;
    int sizeFd = -1;
    int headTailFd = -1;

    uint8_t* frameBuffer = nullptr;
    uint32_t* frameSizes = nullptr;
    uint64_t* headTail = nullptr;
};