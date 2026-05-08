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
// TEMPORAL IMPORTANCE
// --------------------------------------------------

float ImportanceEngine::computeTemporal(
    uint64_t frame,
    uint64_t peak)
{
    float dist =
        std::abs((int64_t)frame - (int64_t)peak);

    return std::exp(-(dist * dist) / 80.0f);
}

// --------------------------------------------------
// MAIN ANALYSIS
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

    // --------------------------------------------------
    // PREPROCESS
    // --------------------------------------------------

    cv::Mat resized;
    cv::Mat gray;

    cv::resize(frame, resized, cv::Size(w, h));
    cv::cvtColor(resized, gray, cv::COLOR_BGR2GRAY);

    // --------------------------------------------------
    // MOTION ANALYSIS
    // --------------------------------------------------

    float motionVal = 0.0f;

    if (!prev.empty())
    {
        cv::Mat prevR;
        cv::Mat prevG;

        cv::resize(prev, prevR, cv::Size(w, h));
        cv::cvtColor(prevR, prevG, cv::COLOR_BGR2GRAY);

        cv::Mat diff;
        cv::absdiff(gray, prevG, diff);

        motionVal =
            cv::mean(diff)[0] / 255.0f;

        // remove sensor noise
        if (motionVal < 0.015f)
            motionVal = 0.0f;
    }

    // EMA smoothing
    s.motion =
        0.7f * prevMotion +
        0.3f * motionVal;

    prevMotion = s.motion;

    // --------------------------------------------------
    // SPATIAL ANALYSIS
    // --------------------------------------------------

    cv::Mat edges;

    cv::Canny(gray, edges, 50, 150);

    float edgeDensity =
        (float)cv::countNonZero(edges)
        / (w * h + 1e-6f);

    s.spatial = edgeDensity;

    // --------------------------------------------------
    // REGION PRIORITY
    // --------------------------------------------------

    cv::Rect center(
        w / 4,
        h / 4,
        w / 2,
        h / 2
    );

    cv::Mat roi = edges(center);

    float regionVal =
        (float)cv::countNonZero(roi)
        / (roi.total() + 1e-6f);

    s.region =
        std::min(1.0f, regionVal * 1.35f);

    s.roiImportance = s.region;

    // --------------------------------------------------
    // TEMPORAL WEIGHTING
    // --------------------------------------------------

    s.temporal =
        computeTemporal(frameIndex, peakFrame);

    // --------------------------------------------------
    // FACE DETECTION
    // --------------------------------------------------

    float faceScore = lastFaceScore;

    if (cfg.enableFace &&
        faceReady &&
        (frameCounter % cfg.faceDetectInterval == 0))
    {
        std::vector<cv::Rect> faces;

        faceCascade.detectMultiScale(
            gray,
            faces,
            1.1,
            3,
            0,
            cv::Size(24, 24)
        );

        s.faces = faces;

        if (!faces.empty())
        {
            float maxArea = 0.0f;

            for (const auto& f : faces)
            {
                maxArea =
                    std::max(maxArea, (float)f.area());
            }

            faceScore =
                std::min(
                    1.0f,
                    maxArea / (w * h * 0.18f)
                );

            // boost ROI importance
            s.roiImportance =
                std::max(s.roiImportance, 0.85f);
        }
        else
        {
            // smooth decay
            faceScore *= 0.85f;
        }

        lastFaceScore = faceScore;
    }

    s.face = faceScore;

    // --------------------------------------------------
    // FINAL FUSED SCORE
    // --------------------------------------------------

    float raw =
        0.30f * s.motion +
        0.20f * s.spatial +
        0.20f * s.region +
        0.20f * s.temporal +
        0.10f * s.face;

    float normalized =
        std::log1p(raw * 7.0f)
        / std::log1p(7.0f);

    s.score =
        std::clamp(normalized, 0.0f, 1.0f);

    return s;
}