#include "SharedFrameBuffer.hpp"

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <cstring>

bool SharedFrameBuffer::initialize()
{
    dataFd = shm_open(FRAME_DATA_NAME, O_RDONLY, 0666);
    sizeFd = shm_open(FRAME_SIZE_NAME, O_RDONLY, 0666);
    headTailFd = shm_open(FRAME_HEADTAIL_NAME, O_RDONLY, 0666);

    if (dataFd < 0 || sizeFd < 0 || headTailFd < 0)
        return false;

    frameBuffer = static_cast<uint8_t*>(
        mmap(
            nullptr,
            BUFFER_SIZE * SLOT_SIZE,
            PROT_READ,
            MAP_SHARED,
            dataFd,
            0
        )
    );

    frameSizes = static_cast<uint32_t*>(
        mmap(
            nullptr,
            BUFFER_SIZE * sizeof(uint32_t),
            PROT_READ,
            MAP_SHARED,
            sizeFd,
            0
        )
    );

    headTail = static_cast<uint64_t*>(
        mmap(
            nullptr,
            sizeof(uint64_t) * 2,
            PROT_READ,
            MAP_SHARED,
            headTailFd,
            0
        )
    );

    return
        frameBuffer != MAP_FAILED &&
        frameSizes != MAP_FAILED &&
        headTail != MAP_FAILED;
}

bool SharedFrameBuffer::isValid() const
{
    return
        frameBuffer &&
        frameSizes &&
        headTail;
}

uint64_t SharedFrameBuffer::latestFrameId() const
{
    return headTail[0];
}

bool SharedFrameBuffer::validateJPEG(
    const std::vector<uint8_t>& data
) const
{
    if (data.size() < 4)
        return false;

    return
        data[0] == 0xFF &&
        data[1] == 0xD8 &&
        data[data.size() - 2] == 0xFF &&
        data[data.size() - 1] == 0xD9;
}

bool SharedFrameBuffer::readFrame(
    uint64_t absoluteFrameId,
    std::vector<uint8_t>& out
)
{
    uint64_t slot = absoluteFrameId % BUFFER_SIZE;

    uint32_t size1 = frameSizes[slot];

    if (size1 == 0 || size1 > SLOT_SIZE)
        return false;

    out.resize(size1);

    memcpy(
        out.data(),
        frameBuffer + (slot * SLOT_SIZE),
        size1
    );

    uint32_t size2 = frameSizes[slot];

    if (size1 != size2)
        return false;

    return validateJPEG(out);
}