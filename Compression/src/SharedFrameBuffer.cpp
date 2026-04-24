#include "SharedFrameBuffer.hpp"
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include "SharedMemoryConfig.hpp"

static constexpr const char* SHM_DATA = "iris_frame_buffer_data";
static constexpr const char* SHM_SIZES = "iris_frame_sizes";
static constexpr const char* SHM_HEAD = "iris_frame_head_tail";

bool SharedFrameBuffer::initialize()
{
    bufferSize = FRAME_BUFFER_SIZE;
    slotSize = SLOT_SIZE;
    return mapMemory();
}

bool SharedFrameBuffer::mapMemory()
{
    fd_data = shm_open(SHM_DATA, O_RDONLY, 0666);
    fd_sizes = shm_open(SHM_SIZES, O_RDONLY, 0666);
    fd_head_tail = shm_open(SHM_HEAD, O_RDONLY, 0666);

    if (fd_data < 0 || fd_sizes < 0 || fd_head_tail < 0)
        return false;

    frame_buffer = (uint8_t*)mmap(nullptr,
        FRAME_BUFFER_SIZE * SLOT_SIZE,
        PROT_READ, MAP_SHARED, fd_data, 0);

    frame_sizes = (uint32_t*)mmap(nullptr,
        FRAME_BUFFER_SIZE * sizeof(uint32_t),
        PROT_READ, MAP_SHARED, fd_sizes, 0);

    head_tail = (uint64_t*)mmap(nullptr,
        2 * sizeof(uint64_t),
        PROT_READ, MAP_SHARED, fd_head_tail, 0);

    return frame_buffer != MAP_FAILED &&
        frame_sizes != MAP_FAILED &&
        head_tail != MAP_FAILED;
}

bool SharedFrameBuffer::isValid() const
{
    return frame_buffer && frame_sizes && head_tail;
}

uint64_t SharedFrameBuffer::getHead() const
{
    return head_tail ? head_tail[0] : 0;
}

uint64_t SharedFrameBuffer::getTail() const
{
    return head_tail ? head_tail[1] : 0;
}

bool SharedFrameBuffer::getFrame(uint64_t index, std::vector<uint8_t>& outJpeg)
{
    if (!isValid())
        return false;

    size_t slot = index % bufferSize;

    uint32_t size = frame_sizes[slot];

    if (size == 0 || size > slotSize)
        return false;

    uint8_t* ptr = frame_buffer + slot * slotSize;

    outJpeg.assign(ptr, ptr + size);
    return true;
}