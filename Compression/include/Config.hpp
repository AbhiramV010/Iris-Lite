#pragma once
#include <string>

struct Config
{
    std::string mode = "production";

    int encodeWidth = 640;
    int encodeHeight = 360;
    int fps = 24;

    int perceptualWidth = 320;
    int perceptualHeight = 180;

    bool useFaces = true;
    bool useMotion = true;
    bool useEdges = true;

    int crf = 26;

    // adaptive compression tuning
    int fpsIdle = 12;
    int fpsMotion = 20;
    int fpsEvent = 24;

    int minKeepFps = 8;
};

bool loadConfig(Config& cfg, const std::string& path);