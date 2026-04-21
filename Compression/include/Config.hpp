#pragma once
#include <string>

struct Config
{
    std::string mode;

    int encodeWidth = 1280;
    int encodeHeight = 720;

    int fps = 24;

    int perceptualWidth = 320;
    int perceptualHeight = 180;

    bool useFaces = true;
    bool useMotion = true;
    bool useEdges = true;

    int crf = 28;

    // event system
    int bufferSize = 300;
    float estimatedLatency = 3.0f;
};

bool loadConfig(Config& cfg, const std::string& path);