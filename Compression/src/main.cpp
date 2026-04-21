#include <iostream>
#include <thread>
#include <chrono>
#include <cstdint>
#include <cstring>

#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

#include "Config.hpp"
#include "CompressionEngine.hpp"
#include "logging.hpp"

struct SharedEventBuffer
{
    uint64_t startFrame;
    uint64_t endFrame;
};

int main()
{
    logInfo("IRIS-Lite Compression Engine starting");

    Config cfg;

    if (!loadConfig(cfg, "config/config.json"))
    {
        logError("Failed to load config.json");
        return -1;
    }

    CompressionEngine engine;

    if (!engine.initialize(cfg))
    {
        logError("Failed to initialize CompressionEngine");
        return -1;
    }

    // ---------------- OPEN SHM ONCE ----------------
    const char* shm_name = "/iris_frame_indices";

    int fd = shm_open(shm_name, O_RDONLY, 0666);
    if (fd < 0)
    {
        logError("Failed to open event shared memory");
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

    logInfo("Listening for events...");

    uint64_t lastStart = 0;
    uint64_t lastEnd = 0;

    while (true)
    {
        uint64_t startFrame = shared->startFrame;
        uint64_t endFrame = shared->endFrame;

        // Only trigger on NEW event
        if (endFrame > startFrame &&
            (startFrame != lastStart || endFrame != lastEnd))
        {
            logInfo("Event: " +
                std::to_string(startFrame) + " → " +
                std::to_string(endFrame));

            engine.processEvent({
                startFrame,
                endFrame,
                "python_trigger"
                });

            lastStart = startFrame;
            lastEnd = endFrame;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // never reached realistically
    munmap(ptr, sizeof(SharedEventBuffer));
    close(fd);

    engine.shutdown();
    return 0;
}