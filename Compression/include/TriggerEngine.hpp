#pragma once
#include <opencv2/opencv.hpp>

class TriggerEngine
{
public:
    struct Result
    {
        float motion = 0.0f;
        bool triggerDeepAnalysis = false;
    };

    Result evaluate(const cv::Mat& frame, const cv::Mat& prev);

    void setFPS(float f) { fps = f; }
    void setMotionThreshold(float t) { motionThreshold = t; }

private:
    float motionThreshold = 0.02f;
    float fps = 24.0f;
};