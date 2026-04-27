#pragma once
#include <string>

struct Config
{
    int encodeWidth = 640;
    int encodeHeight = 360;

    int perceptualWidth = 320;
    int perceptualHeight = 180;

    int fps = 24;
    int baseCRF = 26;

    bool enableFace = true;
    bool enableMotion = true;
    bool enableSpatial = true;
};

bool loadConfig(Config& cfg, const std::string& path);