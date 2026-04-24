#include "ImportanceEngine.hpp"
#include "logging.hpp"

#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <cmath>

ImportanceEngine::ImportanceEngine(int width, int height, const Config& cfg_)
    : w(width), h(height), cfg(cfg_)
{
    logInfo("ImportanceEngine initialized");
}

static float norm(const cv::Mat& m)
{
    return static_cast<float>(cv::mean(m)[0]) / 255.0f;
}

ImportanceSignal ImportanceEngine::analyze(const cv::Mat& frame, const cv::Mat& prev)
{
    ImportanceSignal r{};

    if (frame.empty())
        return r;

    cv::Mat gray, prevGray;
    cv::resize(frame, gray, cv::Size(w, h));
    cv::cvtColor(gray, gray, cv::COLOR_BGR2GRAY);

    if (!prev.empty())
    {
        cv::resize(prev, prevGray, cv::Size(w, h));
        cv::cvtColor(prevGray, prevGray, cv::COLOR_BGR2GRAY);

        cv::Mat diff;
        cv::absdiff(gray, prevGray, diff);
        cv::blur(diff, diff, cv::Size(5, 5));  // cheap
        r.motion = norm(diff);
    }

    r.global = std::clamp(0.8f * prevGlobal + 0.2f * r.motion, 0.0f, 1.0f);
    prevGlobal = r.global;

    return r;
}