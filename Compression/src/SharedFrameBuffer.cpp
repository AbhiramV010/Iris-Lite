#include "SharedFrameBuffer.hpp"
#include "SharedMemoryConfig.hpp"
#include "logging.hpp"

#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <thread>
#include <chrono>
#include <cstring>

static constexpr const char* SHM_DATA = "iris_frame_buffer_data";
static constexpr const char* SHM_SIZES = "iris_frame_sizes";
static constexpr const char* SHM_HEAD = "iris_frame_head_tail";

bool SharedFrameBuffer::initialize()
{
    bufferSize = FRAME_BUFFER_SIZE;
    slotSize = SLOT_SIZE;

    return mapMemory();
}

bool SharedFrameBuffer::isValid() const
{
    return frame_buffer && frame_sizes && head_tail;
}

bool SharedFrameBuffer::mapMemory()
{
    auto waitOpen = [](const char* name)
        {
            int fd;
            int tries = 0;

            while ((fd = shm_open(name, O_RDONLY, 0666)) < 0)
            {
                if (++tries > 50)
                {
                    logError(std::string("SHM failed: ") + name);
                    return -1;
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(200));
            }

            return fd;
        };

    fd_data = waitOpen(SHM_DATA);
    fd_sizes = waitOpen(SHM_SIZES);
    fd_head_tail = waitOpen(SHM_HEAD);

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

    if (frame_buffer == MAP_FAILED ||
        frame_sizes == MAP_FAILED ||
        head_tail == MAP_FAILED)
    {
        logError("mmap failed (SharedFrameBuffer)");
        return false;
    }

    logInfo("SharedFrameBuffer ready");
    return true;
}

bool SharedFrameBuffer::getFrame(uint64_t index, std::vector<uint8_t>& out)
{
    if (!isValid()) return false;

    size_t slot = index % FRAME_BUFFER_SIZE;

    uint32_t size = frame_sizes[slot];
    if (size == 0 || size > SLOT_SIZE)
        return false;

    out.resize(size);

    std::memcpy(out.data(),
        frame_buffer + slot * SLOT_SIZE,
        size);

    return true;
}