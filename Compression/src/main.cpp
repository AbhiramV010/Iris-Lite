#include <iostream>
#include <thread>
#include <chrono>

#include "Config.hpp"
#include "CompressionEngine.hpp"
#include "logging.hpp"

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

    logInfo("Engine running with config.json settings");

    // Keep process alive
    while (true)
    {
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }

    engine.shutdown();

    return 0;
}