#include "Config.hpp"
#include "logging.hpp"
#include <fstream>
#include <iostream>

// JSON library
#include "../external/json.hpp"
using json = nlohmann::json;

bool loadConfig(Config& cfg, const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        logError("Failed to open config file: " + path);
        return false;
    }

    json j;
    try
    {
        file >> j;
    }
    catch (const std::exception& e)
    {
        logError(std::string("JSON parse error: ") + e.what());
        return false;
    }

    // Load values with defaults
    cfg.mode = j.value("mode", "windows");
    cfg.encodeWidth = j.value("encode_width", 640);
    cfg.encodeHeight = j.value("encode_height", 360);

    cfg.perceptualWidth = j.value("perceptual_width", 320);
    cfg.perceptualHeight = j.value("perceptual_height", 180);

    cfg.useFaces = j.value("use_faces", true);
    cfg.useMotion = j.value("use_motion", true);
    cfg.useEdges = j.value("use_edges", true);
    cfg.fps = j.value("fps", 24);
    cfg.crf = j.value("crf", 22);

    // Load privacy zones
    if (j.contains("privacy_zones") && j["privacy_zones"].is_array())
    {
        for (const auto& z : j["privacy_zones"])
        {
            int x = z.value("x", 0);
            int y = z.value("y", 0);
            int w = z.value("w", 0);
            int h = z.value("h", 0);

            cfg.privacyZones.emplace_back(x, y, w, h);
        }

        logInfo("Loaded " + std::to_string(cfg.privacyZones.size()) + " privacy zones");
    }
    else
    {
        logInfo("No privacy zones defined");
    }

    logInfo("Config loaded successfully from " + path);
    return true;
}
