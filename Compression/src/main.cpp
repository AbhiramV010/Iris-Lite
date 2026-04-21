#include <iostream>
#include <thread>
#include <chrono>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

#include "Config.hpp"
#include "CompressionEngine.hpp"
#include "logging.hpp"
#include "Utils.hpp"

int main() {
    logInfo("IRIS-Lite Compression Engine starting");

    Config cfg;

    if (!loadConfig(cfg, "config/config.json"))
    {
        logError("Failed to load config.json");
        return -1;
    }

    CompressionEngine engine(cfg);

    if (!engine.initialize())
    {
        logError("Failed to initialize CompressionEngine");
        return -1;
    }

    logInfo("Background watcher started");
    
    // main.py func
    int indices_fd = shm_open("/iris_frame_indices", O_RDONLY, 0666); // read the 16kb memory loc for frame idx
    
    if (indices_fd != -1) {
        void* ptr = mmap(0, 16, PROT_READ, MAP_SHARED, indices_fd, 0);
    
        if (ptr != MAP_FAILED) {
            uint64_t START_IDX = static_cast<uint64_t*>(ptr)[0];
            uint64_t END_IDX= static_cast<uint64_t*>(ptr)[1];

            munmap(ptr, 16);
        }
        close(indices_fd);
    }
    // end main.py func
    
    const std::string inputFolder = "input";
    const std::string processedFolder = "processed";

    ensureDirectory(processedFolder);

    while (true)
    {
        auto files = listVideoFiles(inputFolder);

        for (const auto& file : files)
        {
            logInfo("Found file: " + file);

            engine.processVideoFile(file);

            // Move processed file
            std::string filename =
                file.substr(file.find_last_of("/\\") + 1);

            std::string dst = processedFolder + "/" + filename;

            if (moveFile(file, dst))
            {
                logInfo("Moved to processed: " + filename);
            }
        }

        std::this_thread::sleep_for(std::chrono::seconds(3));
    }

    engine.shutdown();
    return 0;
}