#include "SharedFrameBuffer.hpp"
#include "SharedMemoryConfig.hpp"
#include "logging.hpp"

#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <cstring>

#include <chrono>
#include <thread>

// --------------------------------------------------
// SHM NAMES
// --------------------------------------------------

static constexpr const char* SHM_NAME = "/iris_live_frame";
static constexpr const char* SIZE_NAME = "iris_frame_sizes";
static constexpr const char* HEAD_NAME = "iris_frame_head_tail";

// --------------------------------------------------
// INIT
// --------------------------------------------------

bool SharedFrameBuffer::initialize()
{
    return mapMemory();
}

bool SharedFrameBuffer::isValid() const
{
    return frame_buffer &&
        frame_buffer != MAP_FAILED &&
        frame_sizes &&
        frame_sizes != MAP_FAILED &&
        head_tail &&
        head_tail != MAP_FAILED;
}

// --------------------------------------------------
// CORE MAPPING
// --------------------------------------------------

bool SharedFrameBuffer::mapMemory()
{
    int tries = 0;

    // -------- WAIT FOR FRAME SHM --------
    while ((fd = shm_open(SHM_NAME, O_RDONLY, 0666)) < 0)
    {
        if (++tries > 200)
        {
            logError("Frame SHM timeout");
            return false;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    // -------- SAFE SIZE COMPUTATION (FIXED OVERFLOW) --------
    size_t totalFrameBytes =
        static_cast<size_t>(FRAME_BUFFER_SIZE) *
        static_cast<size_t>(SLOT_SIZE);

    size_t sizesBytes =
        static_cast<size_t>(FRAME_BUFFER_SIZE) *
        sizeof(uint32_t);

    size_t headBytes = sizeof(uint64_t) * 2;

    // -------- MAP FRAME BUFFER --------
    frame_buffer = static_cast<uint8_t*>(mmap(
        nullptr,
        totalFrameBytes,
        PROT_READ,
        MAP_SHARED,
        fd,
        0
    ));

    if (frame_buffer == MAP_FAILED)
    {
        logError(std::string("mmap frame_buffer failed: ") + std::strerror(errno));
        return false;
    }

    // -------- MAP FRAME SIZES --------
    fd_sizes = shm_open(SIZE_NAME, O_RDONLY, 0666);
    if (fd_sizes < 0)
    {
        logError(std::string("shm_open sizes failed: ") + std::strerror(errno));
        return false;
    }

    frame_sizes = static_cast<uint32_t*>(mmap(
        nullptr,
        sizesBytes,
        PROT_READ,
        MAP_SHARED,
        fd_sizes,
        0
    ));

    if (frame_sizes == MAP_FAILED)
    {
        logError(std::string("mmap frame_sizes failed: ") + std::strerror(errno));
        return false;
    }

    // -------- MAP HEAD/TAIL --------
    fd_head = shm_open(HEAD_NAME, O_RDONLY, 0666);
    if (fd_head < 0)
    {
        logError(std::string("shm_open head_tail failed: ") + std::strerror(errno));
        return false;
    }

    head_tail = static_cast<uint64_t*>(mmap(
        nullptr,
        headBytes,
        PROT_READ,
        MAP_SHARED,
        fd_head,
        0
    ));

    if (head_tail == MAP_FAILED)
    {
        logError(std::string("mmap head_tail failed: ") + std::strerror(errno));
        return false;
    }

    // -------- SUCCESS --------
    logInfo("SharedFrameBuffer mapped (SAFE MODE, NO OVERFLOW)");

    return true;
}

// --------------------------------------------------
// ACCESSORS
// --------------------------------------------------

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