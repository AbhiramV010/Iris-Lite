#include "SharedFrameBuffer.hpp"
#include "logging.hpp"

#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

#include <thread>
#include <chrono>

static constexpr const char* FRAME_DATA_SHM =
"/iris_frame_buffer_data";

static constexpr const char* FRAME_SIZE_SHM =
"/iris_frame_sizes";

static constexpr const char* HEAD_TAIL_SHM =
"/iris_frame_head_tail";

// --------------------------------------------------
// INIT
// --------------------------------------------------

bool SharedFrameBuffer::initialize()
{
    return mapMemory();
}

bool SharedFrameBuffer::isValid() const
{
    return frameBuffer &&
        frameSizes &&
        headTail &&
        frameBuffer != MAP_FAILED &&
        frameSizes != MAP_FAILED &&
        headTail != MAP_FAILED;
}

// --------------------------------------------------
// MAP MEMORY
// --------------------------------------------------

bool SharedFrameBuffer::mapMemory()
{
    int tries = 0;

    while ((dataFd =
        shm_open(FRAME_DATA_SHM, O_RDONLY, 0666)) < 0)
    {
        if (++tries > 200)
        {
            logError("Frame buffer SHM timeout");
            return false;
        }

        std::this_thread::sleep_for(
            std::chrono::milliseconds(50));
    }

    sizeFd =
        shm_open(FRAME_SIZE_SHM, O_RDONLY, 0666);

    headTailFd =
        shm_open(HEAD_TAIL_SHM, O_RDONLY, 0666);

    if (sizeFd < 0 || headTailFd < 0)
    {
        logError("Failed to open SHM metadata");
        return false;
    }

    // --------------------------------------------------
    // MAP FRAME DATA
    // --------------------------------------------------

    frameBuffer = static_cast<uint8_t*>(mmap(
        nullptr,
        BUFFER_SIZE * SLOT_SIZE,
        PROT_READ,
        MAP_SHARED,
        dataFd,
        0
    ));

    // --------------------------------------------------
    // MAP FRAME SIZES
    // --------------------------------------------------

    frameSizes = static_cast<uint32_t*>(mmap(
        nullptr,
        BUFFER_SIZE * sizeof(uint32_t),
        PROT_READ,
        MAP_SHARED,
        sizeFd,
        0
    ));

    // --------------------------------------------------
    // MAP HEAD/TAIL
    // --------------------------------------------------

    headTail = static_cast<uint64_t*>(mmap(
        nullptr,
        sizeof(uint64_t) * 2,
        PROT_READ,
        MAP_SHARED,
        headTailFd,
        0
    ));

    if (!isValid())
    {
        logError("Shared memory mmap failed");
        return false;
    }

    logInfo("SharedFrameBuffer mapped successfully");

    return true;
}

// --------------------------------------------------
// ACCESSORS
// --------------------------------------------------

const uint8_t* SharedFrameBuffer::getFrameData(
    uint64_t index) const
{
    return frameBuffer +
        ((index % BUFFER_SIZE) * SLOT_SIZE);
}

uint32_t SharedFrameBuffer::getFrameSize(
    uint64_t index) const
{
    uint32_t size =
        frameSizes[index % BUFFER_SIZE];

    // HARD SAFETY CHECK
    if (size == 0 || size > SLOT_SIZE)
        return 0;

    return size;
}