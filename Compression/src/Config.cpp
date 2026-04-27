#include "Config.hpp"
#include "logging.hpp"
#include "json.hpp"

#include <fstream>

using json = nlohmann::json;

bool loadConfig(Config& cfg, const std::string& path)
{
    std::ifstream f(path);
    if (!f.is_open())
    {
        logError("Config missing: " + path);
        return false;
    }

    try
    {
        json j;
        f >> j;

        cfg.encodeWidth = j.value("encode_width", cfg.encodeWidth);
        cfg.encodeHeight = j.value("encode_height", cfg.encodeHeight);

        cfg.perceptualWidth = j.value("perceptual_width", cfg.perceptualWidth);
        cfg.perceptualHeight = j.value("perceptual_height", cfg.perceptualHeight);

        cfg.fps = j.value("fps", cfg.fps);
        cfg.baseCRF = j.value("crf", cfg.baseCRF);

        logInfo("Config loaded successfully");
        return true;
    }
    catch (const std::exception& e)
    {
        logError(std::string("Config parse error: ") + e.what());
        return false;
    }
}