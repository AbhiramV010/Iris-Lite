#include <iostream>
#include <thread>
#include <chrono>
#include <cstdint>

#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

#include "Config.hpp"
#include "CompressionEngine.hpp"
#include "logging.hpp"

struct SharedEventBuffer {
    uint64_t startFrame;
    uint64_t endFrame;
};

int main()
{
    logInfo("Iris-Lite Compression Engine starting");

    Config cfg;
    if (!loadConfig(cfg, "config/config.json"))
    {
        logError("Failed to load config.json");
        return -1;
    }

    CompressionEngine engine;
    if (!engine.initialize(cfg))
    {
        logError("Failed to initialize engine");
        return -1;
    }

    int fd = shm_open("iris_concern_indices", O_RDONLY, 0666);
    if (fd < 0)
    {
        logError("Failed to open shared memory");
        return -1;
    }

    void* ptr = mmap(nullptr, sizeof(SharedEventBuffer),
        PROT_READ, MAP_SHARED, fd, 0);

    if (ptr == MAP_FAILED)
    {
        logError("mmap failed");
        close(fd);
        return -1;
    }

    auto* shared = reinterpret_cast<SharedEventBuffer*>(ptr);

    uint64_t lastStart = 0;
    uint64_t lastEnd = 0;

    logInfo("Listening for events...");

    while (true)
    {
        uint64_t start = shared->startFrame;
        uint64_t end = shared->endFrame;

        if (start != lastStart || end != lastEnd)
        {
            lastStart = start;
            lastEnd = end;

            if (end > start)
            {
                EventWindow event;
                event.startFrame = start;
                event.endFrame = end;
                event.trigger = "python_event";

                engine.processEvent(event);
            }
            else
            {
                logError("Invalid frame window");
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    munmap(ptr, sizeof(SharedEventBuffer));
    close(fd);

    engine.shutdown();
    return 0;
}