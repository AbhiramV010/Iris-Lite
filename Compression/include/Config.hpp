#pragma once
#include <string>

struct Config
{
    // ---------------- VIDEO ----------------
    int encodeWidth = 640;
    int encodeHeight = 360;

    int perceptualWidth = 320;
    int perceptualHeight = 180;

    int fps = 24;
    int baseCRF = 22;

    // ---------------- FEATURES ----------------
    bool enableFace = true;
    bool enableMotion = true;
    bool enableSpatial = true;

    // ---------------- FACE DETECTION ----------------
    std::string faceModelPath =
        "/usr/share/opencv4/haarcascades/haarcascade_frontalface_default.xml";

    int faceDetectInterval = 10;   // run every N frames

    // ---------------- FUTURE EXTENSIONS ----------------
    int maxEventLengthSec = 10;
};

bool loadConfig(Config& cfg, const std::string& path);