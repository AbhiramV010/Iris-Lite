#include "Config.hpp"
#include "logging.hpp"
#include "json.hpp"

#include <fstream>

using json = nlohmann::json;

bool loadConfig(Config& cfg, const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        logError("Failed to open config: " + path);
        return false;
    }

    json j;
    file >> j;

    cfg.mode = j.value("mode", "production");

    cfg.encodeWidth = j.value("encode_width", 640);
    cfg.encodeHeight = j.value("encode_height", 360);
    cfg.fps = j.value("fps", 24);

    cfg.perceptualWidth = j.value("perceptual_width", 320);
    cfg.perceptualHeight = j.value("perceptual_height", 180);

    cfg.useFaces = j.value("use_faces", true);
    cfg.useMotion = j.value("use_motion", true);
    cfg.useEdges = j.value("use_edges", true);

    cfg.crf = j.value("crf", 26);

    cfg.fpsIdle = j.value("fps_idle", 12);
    cfg.fpsMotion = j.value("fps_motion", 20);
    cfg.fpsEvent = j.value("fps_event", 24);

    cfg.minKeepFps = j.value("min_keep_fps", 8);

    return true;
}