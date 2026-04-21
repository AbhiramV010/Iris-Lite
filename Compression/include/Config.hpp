#pragma once
#include <string>
#include <vector>
#include <opencv2/core.hpp>

struct Config
{
    std::string mode;

    int encodeWidth;
    int encodeHeight;
    int fps;
    int perceptualWidth;
    int perceptualHeight;

    bool useFaces;
    bool useMotion;
    bool useEdges;

    int crf;

};

bool loadConfig(Config& cfg, const std::string& path);
