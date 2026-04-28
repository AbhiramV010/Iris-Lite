#include "ImportanceEngine.hpp"
#include "logging.hpp"

#include <opencv2/imgproc.hpp>
#include <cmath>
#include <algorithm>

ImportanceEngine::ImportanceEngine(int w_, int h_, const Config& cfg_)
    : w(w_), h(h_), cfg(cfg_)
{
}

float ImportanceEngine::computeTemporal(uint64_t frame, uint64_t peak)
{
    float dist = std::abs((int64_t)frame - (int64_t)peak);

    // sharper center emphasis (keeps event locality strong)
    return std::exp(-(dist * dist) / 120.0f);
}

ImportanceSignal ImportanceEngine::analyze(
    const cv::Mat& frame,
    const cv::Mat& prev,
    uint64_t frameIndex,
    uint64_t peakFrame)
{
    ImportanceSignal s{};

    if (frame.empty())
        return s;

    // ---------------- PREPROCESS ----------------
    cv::Mat resized, gray;
    cv::resize(frame, resized, cv::Size(w, h));
    cv::cvtColor(resized, gray, cv::COLOR_BGR2GRAY);

    // ---------------- MOTION ----------------
    float motionVal = 0.0f;

    if (!prev.empty())
    {
        cv::Mat prevR, prevG;
        cv::resize(prev, prevR, cv::Size(w, h));
        cv::cvtColor(prevR, prevG, cv::COLOR_BGR2GRAY);

        cv::Mat diff;
        cv::absdiff(gray, prevG, diff);

        motionVal = cv::mean(diff)[0] / 255.0f;
    }

    s.motion = 0.80f * prevMotion + 0.20f * motionVal;
    prevMotion = s.motion;

    // ---------------- SPATIAL DETAIL ----------------
    cv::Mat edges;
    cv::Canny(gray, edges, 50, 150);

    s.spatial =
        static_cast<float>(cv::countNonZero(edges)) /
        (w * h + 1e-6f);

    // ---------------- REGION (SAFE ROI) ----------------
    int cx = std::max(0, w / 4);
    int cy = std::max(0, h / 4);
    int cw = std::min(w / 2, w - cx);
    int ch = std::min(h / 2, h - cy);

    cv::Rect center(cx, cy, cw, ch);

    float regionVal = 0.0f;
    if (center.width > 0 && center.height > 0)
    {
        cv::Mat roiEdges = edges(center);
        regionVal =
            static_cast<float>(cv::countNonZero(roiEdges)) /
            (cw * ch + 1e-6f);
    }

    s.region = regionVal;

    // ---------------- TEMPORAL ----------------
    float dist = std::abs((int64_t)frameIndex - (int64_t)peakFrame);
    s.temporal = std::exp(-(dist * dist) / 140.0f);

    // ---------------- FACE (DISABLED BUT STABLE) ----------------
    // Keep field alive for future Haar integration, but never assume runtime usage
    s.face = 0.0f;

    // ---------------- PERCEPTUAL FUSION ----------------
    float raw =
        0.60f * s.motion +
        0.25f * s.spatial +
        0.10f * s.region +
        0.05f * s.temporal;

    float normalized =
        std::log1p(raw * 6.5f) / std::log1p(6.5f);

    s.score = std::clamp(normalized, 0.0f, 1.0f);

    // ---------------- CONTROLLED LOGGING ----------------
    if (frameIndex % 40 == 0)
    {
        logInfo(
            "IMP | m=" + std::to_string(s.motion) +
            " s=" + std::to_string(s.spatial) +
            " r=" + std::to_string(s.region) +
            " t=" + std::to_string(s.temporal) +
            " score=" + std::to_string(s.score)
        );
    }

    return s;
}