#include "ImportanceEngine.hpp"
#include <opencv2/imgproc.hpp>
#include <algorithm>

ImportanceEngine::ImportanceEngine(int w_, int h_, const Config& cfg_)
    : w(w_), h(h_), cfg(cfg_) {
}

ImportanceSignal ImportanceEngine::analyze(const cv::Mat& frame,
    const cv::Mat& prev)
{
    ImportanceSignal s;

    if (frame.empty()) return s;

    cv::Mat resized, gray;
    cv::resize(frame, resized, cv::Size(w, h));
    cv::cvtColor(resized, gray, cv::COLOR_BGR2GRAY);

    // ---------------- MOTION ----------------
    if (!prev.empty())
    {
        cv::Mat prevR, prevG;
        cv::resize(prev, prevR, cv::Size(w, h));
        cv::cvtColor(prevR, prevG, cv::COLOR_BGR2GRAY);

        cv::Mat diff;
        cv::absdiff(gray, prevG, diff);

        float motion = cv::mean(diff)[0] / 255.0f;

        // temporal smoothing
        s.motion = 0.8f * prevMotion + 0.2f * motion;
        prevMotion = s.motion;
    }

    // ---------------- SPATIAL ----------------
    cv::Mat edges;
    cv::Canny(gray, edges, 50, 150);
    s.spatial = (float)cv::countNonZero(edges) /
        (w * h);

    // ---------------- REGION ----------------
    cv::Rect center(w * 0.25, h * 0.25, w * 0.5, h * 0.5);
    cv::Mat roi = gray(center);
    s.region = cv::mean(roi)[0] / 255.0f;

    return s;
}