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

struct SharedEventBuffer {
    uint64_t startFrame;
    uint64_t endFrame;
};

int main() {
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

    const char* shm_name = "/iris_frame_indices";

    int fd = shm_open(shm_name, O_RDONLY, 0666);
    if (fd < 0) {
        logError("Failed to open event shared memory");
        return -1;
    }

    void* ptr = mmap(nullptr, sizeof(SharedEventBuffer),
        PROT_READ, MAP_SHARED, fd, 0);

    if (ptr == MAP_FAILED) {
        logError("mmap failed");
        close(fd);
        return -1;
    }

    auto* shared = reinterpret_cast<SharedEventBuffer*>(ptr);

    logInfo("Listening for events...");

    // main.py func
    while (true) {
        uint64_t START_IDX = shared->startFrame;
        uint64_t END_IDX = shared->endFrame;
        
        std::cout << "START INDEX >> " << START_IDX << std::endl;
        std::cout << "END INDEX >> " << END_IDX << std::endl;

        auto files = listVideoFiles(cfg.inputFolder);
        for (const auto& file : files) {
            if (END_IDX > START_IDX) {
                engine.processVideoClip(file, START_IDX, END_IDX);
            }
            else {
                throw std::invalid_argument("END_IDX must be greater than START_IDX");
            }
            
            moveFile(file, cfg.processedFolder + "/" + getFilename(file));
        }
        std::this_thread::sleep_for(std::chrono::seconds(3));
    }
    // end main.py func

    munmap(ptr, sizeof(SharedEventBuffer));
    close(fd);

    engine.shutdown();
    return 0;
}