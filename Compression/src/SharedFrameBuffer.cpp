#include "SharedFrameBuffer.hpp"
#include "SharedMemoryConfig.hpp"
#include "logging.hpp"

#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <chrono>
#include <thread>

static constexpr const char* SHM_NAME = "/iris_live_frame";
static constexpr const char* SIZE_NAME = "iris_frame_sizes";
static constexpr const char* HEAD_NAME = "iris_frame_head_tail";

bool SharedFrameBuffer::initialize()
{
    return mapMemory();
}

bool SharedFrameBuffer::isValid() const
{
    return frame_buffer &&
        frame_buffer != MAP_FAILED &&
        frame_sizes &&
        head_tail;
}

bool SharedFrameBuffer::mapMemory()
{
    int tries = 0;

    while ((fd = shm_open(SHM_NAME, O_RDONLY, 0666)) < 0)
    {
        if (++tries > 200)
        {
            logError("Frame SHM timeout");
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    frame_buffer = (uint8_t*)mmap(
        nullptr,
        FRAME_BUFFER_SIZE * SLOT_SIZE,
        PROT_READ,
        MAP_SHARED,
        fd,
        0
    );

    fd_sizes = shm_open(SIZE_NAME, O_RDONLY, 0666);
    frame_sizes = (uint32_t*)mmap(
        nullptr,
        FRAME_BUFFER_SIZE * sizeof(uint32_t),
        PROT_READ,
        MAP_SHARED,
        fd_sizes,
        0
    );

    fd_head = shm_open(HEAD_NAME, O_RDONLY, 0666);
    head_tail = (uint64_t*)mmap(
        nullptr,
        16,
        PROT_READ,
        MAP_SHARED,
        fd_head,
        0
    );

    if (frame_buffer == MAP_FAILED ||
        frame_sizes == MAP_FAILED ||
        head_tail == MAP_FAILED)
    {
        logError("SHM mapping failed");
        return false;
    }

    logInfo("SharedFrameBuffer mapped (RING BUFFER ENABLED)");
    return true;
}

uint8_t* SharedFrameBuffer::getFrameBufferBase()
{
    return frame_buffer;
}

uint32_t* SharedFrameBuffer::getFrameSizes()
{
    return frame_sizes;
}

uint64_t* SharedFrameBuffer::getHeadTail()
{
    return head_tail;
}
