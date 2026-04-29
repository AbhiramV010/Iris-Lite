#include "SharedFrameBuffer.hpp"
#include "SharedMemoryConfig.hpp"
#include "logging.hpp"

#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <thread>
#include <chrono>

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
            logError("SharedFrameBuffer: timeout waiting for producer");
            return false;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    logInfo("SharedFrameBuffer: shm_open success");

    frame_buffer = (uint8_t*)mmap(
        nullptr,
        FRAME_SIZE,
        PROT_READ,
        MAP_SHARED,
        fd,
        0
    );

    if (frame_buffer == MAP_FAILED)
    {
        logError("SharedFrameBuffer: mmap failed");
        return false;
    }

    logInfo("SharedFrameBuffer: mapped successfully (" + std::to_string(FRAME_SIZE) + " bytes)");

    return true;
}

uint8_t* SharedFrameBuffer::getFramePtr()
{
    return frame_buffer;
}