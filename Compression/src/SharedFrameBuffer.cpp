#include "SharedFrameBuffer.hpp"
#include "logging.hpp"

#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <thread>
#include <chrono>

static constexpr const char* SHM_NAME = "/iris_frame_buffer_data";

bool SharedFrameBuffer::initialize()
{
    return mapMemory();
}

bool SharedFrameBuffer::isValid() const
{
    return buffer && buffer != MAP_FAILED;
}

bool SharedFrameBuffer::mapMemory()
{
    int tries = 0;

    while ((fd = shm_open(SHM_NAME, O_RDONLY, 0666)) < 0)
    {
        if (++tries > 200)
        {
            logError("SHM timeout");
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    buffer = (FrameSlot*)mmap(
        nullptr,
        sizeof(FrameSlot) * BUFFER_SIZE,
        PROT_READ,
        MAP_SHARED,
        fd,
        0
    );

    if (buffer == MAP_FAILED)
    {
        logError("mmap failed (FrameSlot buffer)");
        return false;
    }

    logInfo("SharedFrameBuffer mapped (RING BUFFER MODE)");
    return true;
}

const FrameSlot* SharedFrameBuffer::getSlot(uint64_t index) const
{
    return &buffer[index % BUFFER_SIZE];
}