#include "SharedFrameBuffer.hpp"
#include "SharedMemoryConfig.hpp"
#include "logging.hpp"

#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <chrono>
#include <thread>
#include <cstring>

static constexpr const char* SHM_NAME = "/iris_live_frame";

bool SharedFrameBuffer::initialize()
{
    return mapMemory();
}

bool SharedFrameBuffer::isValid() const
{
    return frame_buffer && frame_buffer != MAP_FAILED;
}

bool SharedFrameBuffer::mapMemory()
{
    int tries = 0;

    while ((fd = shm_open(SHM_NAME, O_RDONLY, 0666)) < 0)
    {
        if (++tries > 200)
        {
            logError("Frame SHM timeout (Python writer not running)");
            return false;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    size_t size =
        FRAME_WIDTH * FRAME_HEIGHT * FRAME_CHANNELS;

    frame_buffer = static_cast<uint8_t*>(mmap(
        nullptr,
        size,
        PROT_READ,
        MAP_SHARED,
        fd,
        0
    ));

    if (frame_buffer == MAP_FAILED)
    {
        logError(std::string("mmap failed: ") + std::strerror(errno));
        return false;
    }

    logInfo("SharedFrameBuffer mapped (SINGLE FRAME MODE)");
    return true;
}

uint8_t* SharedFrameBuffer::getFrameBufferBase()
{
    return frame_buffer;
}