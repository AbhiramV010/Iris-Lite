#include <iostream>
#include <thread>
#include <chrono>
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
    char trigger[64];
};

static bool waitForSHM(const char* name, int& fd)
{
    int tries = 0;

    while ((fd = shm_open(name, O_RDONLY, 0666)) < 0)
    {
        if (++tries > 100)
        {
            logError(std::string("SHM timeout: ") + name);
            return false;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    return true;
}

int main()
{
    logInfo("Booting Iris-Lite");

    Config cfg;
    if (!loadConfig(cfg, "config/config.json"))
        return -1;

    CompressionEngine engine;
    if (!engine.initialize(cfg))
        return -1;

    int fd;
    if (!waitForSHM("iris_concern_indices", fd))
        return -1;

    void* ptr = mmap(nullptr, sizeof(SharedEventBuffer),
        PROT_READ, MAP_SHARED, fd, 0);

    if (ptr == MAP_FAILED)
    {
        logError("SHM mapping failed");
        return -1;
    }

    auto* shared = reinterpret_cast<SharedEventBuffer*>(ptr);

    uint64_t lastStart = 0, lastEnd = 0;

    logInfo("System online");

    while (true)
    {
        uint64_t start = shared->startFrame;
        uint64_t end = shared->endFrame;

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

        std::string trigger = "unknown";

        if (shared->trigger[0] != '\0')
            trigger = std::string(shared->trigger);

        engine.enqueueEvent({ start, end, trigger });

        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }
}