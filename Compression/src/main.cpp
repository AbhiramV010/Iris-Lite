#include <iostream>
#include <thread>
#include <chrono>
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

    // ---------------- SAFE SHM OPEN (RETRY LOOP) ----------------
    int fd;
    while (true)
    {
        fd = shm_open("iris_concern_indices", O_RDONLY, 0666);
        if (fd >= 0)
            break;

        logWarn("Waiting for iris_concern_indices...");
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    void* ptr;
    while (true)
    {
        ptr = mmap(nullptr, sizeof(SharedEventBuffer),
            PROT_READ, MAP_SHARED, fd, 0);

        if (ptr != MAP_FAILED)
            break;

        logWarn("Waiting for shared memory map...");
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    auto* shared = reinterpret_cast<SharedEventBuffer*>(ptr);

    uint64_t lastStart = 0;
    uint64_t lastEnd = 0;

    // ---------------- MAIN LOOP ----------------
    while (true)
    {
        uint64_t start = shared->startFrame;
        uint64_t end = shared->endFrame;

        // no data → sleep (IMPORTANT for CPU)
        if (start == 0 && end == 0)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        if (start == lastStart && end == lastEnd)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        lastStart = start;
        lastEnd = end;

        EventWindow event{ start, end, "external" };
        engine.enqueueEvent(event);

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    munmap(ptr, sizeof(SharedEventBuffer));
    close(fd);

    engine.shutdown();
    return 0;
}