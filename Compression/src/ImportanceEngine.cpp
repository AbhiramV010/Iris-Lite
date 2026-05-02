#include "ImportanceEngine.hpp"
#include "logging.hpp"

#include <opencv2/imgproc.hpp>
#include <cmath>
#include <algorithm>

// --------------------------------------------------
// INIT
// --------------------------------------------------

ImportanceEngine::ImportanceEngine(int w_, int h_, const Config& cfg_)
    : w(w_), h(h_), cfg(cfg_)
{
    if (cfg.enableFace)
    {
        faceReady = faceCascade.load(cfg.faceModelPath);

        if (faceReady)
            logInfo("Face detection ENABLED");
        else
            logWarn("Face detection FAILED to load model");
    }
}

// --------------------------------------------------
// TEMPORAL
// --------------------------------------------------

float ImportanceEngine::computeTemporal(uint64_t frame, uint64_t peak)
{
    float dist = std::abs((int64_t)frame - (int64_t)peak);

    // Sharper event peak (more perceptual focus)
    return std::exp(-(dist * dist) / 60.0f);
}

// --------------------------------------------------
// ANALYZE
// --------------------------------------------------

ImportanceSignal ImportanceEngine::analyze(
    const cv::Mat& frame,
    const cv::Mat& prev,
    uint64_t frameIndex,
    uint64_t peakFrame)
{
    ImportanceSignal s{};

    if (frame.empty())
        return s;

    frameCounter++;

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

        // Remove sensor noise / flicker
        if (motionVal < 0.02f)
            motionVal = 0.0f;
    }

    s.motion = 0.8f * prevMotion + 0.2f * motionVal;
    prevMotion = s.motion;

    // ---------------- SPATIAL ----------------
    cv::Mat edges;
    cv::Canny(gray, edges, 50, 150);

    s.spatial =
        (float)cv::countNonZero(edges) / (w * h + 1e-6f);

    // ---------------- REGION (CENTER PRIORITY) ----------------
    cv::Rect center(w / 4, h / 4, w / 2, h / 2);

    cv::Mat roi = edges(center);

    float regionVal =
        (float)cv::countNonZero(roi) / (roi.total() + 1e-6f);

    // Slight bias toward center activity
    s.region = std::min(1.0f, regionVal * 1.2f);

    // ---------------- TEMPORAL ----------------
    s.temporal = computeTemporal(frameIndex, peakFrame);

    // ---------------- FACE ----------------
    float faceScore = lastFaceScore;

    if (cfg.enableFace && faceReady &&
        (frameCounter % cfg.faceDetectInterval == 0))
    {
        std::vector<cv::Rect> faces;

        faceCascade.detectMultiScale(gray, faces, 1.1, 3);

        if (!faces.empty())
        {
            float maxArea = 0.0f;
            for (auto& f : faces)
                maxArea = std::max(maxArea, (float)f.area());

            faceScore = std::min(1.0f, maxArea / (w * h * 0.25f));
        }
        else
        {
            faceScore *= 0.9f;
        }

        lastFaceScore = faceScore;
    }

    s.face = faceScore;

    // ---------------- FINAL SCORE ----------------
    float raw =
        0.35f * s.motion +
        0.20f * s.spatial +
        0.10f * s.region +
        0.25f * s.temporal +
        0.10f * s.face;

    float normalized =
        std::log1p(raw * 6.5f) / std::log1p(6.5f);

    s.score = std::clamp(normalized, 0.0f, 1.0f);

    return s;
}