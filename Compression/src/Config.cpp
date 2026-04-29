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

        // ---------------- VIDEO ----------------
        cfg.encodeWidth = j.value("encode_width", cfg.encodeWidth);
        cfg.encodeHeight = j.value("encode_height", cfg.encodeHeight);

        cfg.perceptualWidth = j.value("perceptual_width", cfg.perceptualWidth);
        cfg.perceptualHeight = j.value("perceptual_height", cfg.perceptualHeight);

        cfg.fps = j.value("fps", cfg.fps);
        cfg.baseCRF = j.value("crf", cfg.baseCRF);

        // ---------------- FEATURES ----------------
        cfg.enableFace = j.value("use_faces", cfg.enableFace);
        cfg.enableMotion = j.value("use_motion", cfg.enableMotion);
        cfg.enableSpatial = j.value("use_edges", cfg.enableSpatial);

        // ---------------- FACE ----------------
        cfg.faceModelPath = j.value("face_model_path", cfg.faceModelPath);
        cfg.faceDetectInterval = j.value("face_detect_interval", cfg.faceDetectInterval);

        // ---------------- SANITY CHECKS ----------------
        if (cfg.encodeWidth <= 0 || cfg.encodeHeight <= 0)
        {
            logError("Invalid encode resolution");
            return false;
        }

        if (cfg.fps <= 0 || cfg.fps > 60)
        {
            logWarn("FPS out of range, resetting to 24");
            cfg.fps = 24;
        }

        if (cfg.faceDetectInterval <= 0)
        {
            cfg.faceDetectInterval = 10;
        }

        logInfo("Config loaded successfully");
        return true;
    }
    catch (const std::exception& e)
    {
        logError(std::string("Config parse error: ") + e.what());
        return false;
    }
}