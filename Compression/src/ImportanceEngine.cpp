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
    return std::exp(-(dist * dist) / 120.0f);
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

    // ---------------- SPATIAL ----------------
    cv::Mat edges;
    cv::Canny(gray, edges, 50, 150);

    s.spatial =
        static_cast<float>(cv::countNonZero(edges)) /
        (w * h + 1e-6f);

    // ---------------- REGION ----------------
    int cx = w / 4;
    int cy = h / 4;
    int cw = w / 2;
    int ch = h / 2;

    cv::Rect center(cx, cy, cw, ch);

    float regionVal = 0.0f;
    if (center.width > 0 && center.height > 0)
    {
        cv::Mat roi = edges(center);
        regionVal =
            static_cast<float>(cv::countNonZero(roi)) /
            (cw * ch + 1e-6f);
    }

    s.region = regionVal;

    // ---------------- TEMPORAL ----------------
    s.temporal = computeTemporal(frameIndex, peakFrame);

    // ---------------- FACE DETECTION (THROTTLED) ----------------
    float faceScore = lastFaceScore;

    if (cfg.enableFace && faceReady &&
        (frameCounter % cfg.faceDetectInterval == 0))
    {
        std::vector<cv::Rect> faces;

        faceCascade.detectMultiScale(
            gray,
            faces,
            1.1,
            3,
            0,
            cv::Size(20, 20)
        );

        if (!faces.empty())
        {
            // normalize by area
            float maxArea = 0.0f;
            for (const auto& f : faces)
                maxArea = std::max(maxArea, (float)(f.area()));

            faceScore = std::min(1.0f, maxArea / (w * h * 0.25f));
        }
        else
        {
            faceScore *= 0.9f; // decay instead of hard drop
        }

        lastFaceScore = faceScore;
    }

    s.face = faceScore;

    // ---------------- FUSION ----------------
    float raw =
        0.50f * s.motion +
        0.20f * s.spatial +
        0.10f * s.region +
        0.10f * s.temporal +
        0.10f * s.face;

    float normalized =
        std::log1p(raw * 6.5f) / std::log1p(6.5f);

    s.score = std::clamp(normalized, 0.0f, 1.0f);

    // ---------------- LOG ----------------
    if (frameIndex % 40 == 0)
    {
        logInfo(
            "IMP | m=" + std::to_string(s.motion) +
            " s=" + std::to_string(s.spatial) +
            " f=" + std::to_string(s.face) +
            " score=" + std::to_string(s.score)
        );
    }

    return s;
}