#include <iostream>
#include <thread>
#include <chrono>
#include <cstdint>

#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

#include "Config.hpp"
#include "CompressionEngine.hpp"
#include "SharedMemoryConfig.hpp"
#include "logging.hpp"

struct SharedEventBuffer
{
    uint64_t startFrame;
    uint64_t endFrame;
};

int main()
{
    logInfo("Iris-Lite starting");

    Config cfg;
    if (!loadConfig(cfg, "config/config.json"))
        return -1;

    CompressionEngine engine;
    if (!engine.initialize(cfg))
        return -1;

    int fd = shm_open("iris_concern_indices", O_RDONLY, 0666);
    if (fd < 0)
        return -1;

    void* ptr = mmap(nullptr, sizeof(SharedEventBuffer),
        PROT_READ, MAP_SHARED, fd, 0);

    if (ptr == MAP_FAILED)
        return -1;

    auto* shared = reinterpret_cast<SharedEventBuffer*>(ptr);

    uint64_t lastStart = 0;
    uint64_t lastEnd = 0;

    logInfo("Listening...");

    while (true)
    {
        uint64_t start = shared->startFrame;
        uint64_t end = shared->endFrame;

        // validation
        if (start == 0 && end == 0)
            continue;

        if (start == end)
            continue;

        if (start == lastStart && end == lastEnd)
            continue;

        uint64_t diff = (start > end) ? (start - end) : (end - start);
        if (diff > FRAME_BUFFER_SIZE)
            continue;

        lastStart = start;
        lastEnd = end;

        EventWindow event;
        event.startFrame = start;
        event.endFrame = end;
        event.trigger = "external";

        engine.enqueueEvent(event);

        engine.processQueuedEvents();

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    munmap(ptr, sizeof(SharedEventBuffer));
    close(fd);

    engine.shutdown();
    return 0;
}