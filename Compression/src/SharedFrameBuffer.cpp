#include "SharedFrameBuffer.hpp"
#include "SharedMemoryConfig.hpp"

#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <thread>
#include <chrono>
#include <cstring>
#include <vector>

static constexpr const char* SHM_DATA = "iris_frame_buffer_data";
static constexpr const char* SHM_SIZES = "iris_frame_sizes";
static constexpr const char* SHM_HEAD = "iris_frame_head_tail";

// ---------------- INIT ----------------
bool SharedFrameBuffer::initialize()
{
    bufferSize = FRAME_BUFFER_SIZE;
    slotSize = SLOT_SIZE;
    return mapMemory();
}

// ---------------- MAP MEMORY ----------------
bool SharedFrameBuffer::mapMemory()
{
    while ((fd_data = shm_open(SHM_DATA, O_RDONLY, 0666)) < 0)
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

    while ((fd_sizes = shm_open(SHM_SIZES, O_RDONLY, 0666)) < 0)
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

    while ((fd_head_tail = shm_open(SHM_HEAD, O_RDONLY, 0666)) < 0)
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

    frame_buffer = (uint8_t*)mmap(
        nullptr,
        FRAME_BUFFER_SIZE * SLOT_SIZE,
        PROT_READ,
        MAP_SHARED,
        fd_data,
        0
    );

    frame_sizes = (uint32_t*)mmap(
        nullptr,
        FRAME_BUFFER_SIZE * sizeof(uint32_t),
        PROT_READ,
        MAP_SHARED,
        fd_sizes,
        0
    );

    head_tail = (uint64_t*)mmap(
        nullptr,
        2 * sizeof(uint64_t),
        PROT_READ,
        MAP_SHARED,
        fd_head_tail,
        0
    );

    if (frame_buffer == MAP_FAILED ||
        frame_sizes == MAP_FAILED ||
        head_tail == MAP_FAILED)
    {
        return false;
    }

    return true;
}

// ---------------- GET FRAME (FIX LINKER ERROR) ----------------
bool SharedFrameBuffer::getFrame(uint64_t index, std::vector<uint8_t>& outJpeg)
{
    if (!frame_buffer || !frame_sizes)
        return false;

    size_t slot = index % FRAME_BUFFER_SIZE;

    uint32_t size = frame_sizes[slot];

    if (size == 0 || size > SLOT_SIZE)
        return false;

    outJpeg.resize(size);

    std::memcpy(
        outJpeg.data(),
        frame_buffer + slot * SLOT_SIZE,
        size
    );

    return true;
}