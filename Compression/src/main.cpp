#include <iostream>
#include <thread>
#include <chrono>

#include "Config.hpp"
#include "CompressionEngine.hpp"
#include "logging.hpp"
#include "Utils.hpp"

int main()
{
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

        // 🔥 IMPORTANT: don't hammer CPU
        std::this_thread::sleep_for(std::chrono::seconds(3));
    }

    engine.shutdown();
    return 0;
}