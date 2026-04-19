#include "TriggerEngine.hpp"
#include <opencv2/opencv.hpp>

TriggerEngine::Result TriggerEngine::evaluate(
    const cv::Mat& frame,
    const cv::Mat& prev)

{

    Result r;

    if (frame.empty() || prev.empty())
    {
        r.motion = 0.0f;
        r.triggerDeepAnalysis = true; // fallback safety
        return r;
    }

    cv::Mat diff;
    cv::absdiff(prev, frame, diff);

    cv::Scalar m = cv::mean(diff);

    r.motion =
        static_cast<float>((m[0] + m[1] + m[2]) / 3.0 / 255.0);

    // 🔥 FAST GATE DECISION
    r.triggerDeepAnalysis = (r.motion > motionThreshold);

    return r;
}