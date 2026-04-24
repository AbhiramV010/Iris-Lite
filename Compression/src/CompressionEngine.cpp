#include "ImportanceEngine.hpp"
#include "logging.hpp"

#include <opencv2/imgproc.hpp>
#include <opencv2/objdetect.hpp>
#include <algorithm>
#include <cmath>

ImportanceEngine::ImportanceEngine(int width, int height, const Config& cfg_)
    : w(width), h(height), cfg(cfg_)
{
    logInfo("ImportanceEngine initialized");

    if (cfg.useFaces)
    {
        faceCascade.load("/usr/share/opencv4/haarcascades/haarcascade_frontalface_default.xml");
        if (faceCascade.empty())
            logError("Failed to load Haar cascade");
    }
}

static float normMean(const cv::Mat& m)
{
    return static_cast<float>(cv::mean(m)[0]) / 255.0f;
}

ImportanceSignal ImportanceEngine::analyze(
    const cv::Mat& frame,
    const cv::Mat& prev,
    float load
)
{
    ImportanceSignal r{};

    if (frame.empty())
        return r;

    // -------- PREPROCESS --------
    cv::Mat resized, gray;
    cv::resize(frame, resized, cv::Size(w, h));
    cv::cvtColor(resized, gray, cv::COLOR_BGR2GRAY);

    cv::Mat prevResized, prevGray;
    const cv::Mat& safePrev = prev.empty() ? frame : prev;

    cv::resize(safePrev, prevResized, cv::Size(w, h));
    cv::cvtColor(prevResized, prevGray, cv::COLOR_BGR2GRAY);

    // -------- ADAPTIVE MODES --------
    bool useEdges = false;
    bool useFaces = false;

    if (load < 0.7f)
    {
        // FULL MODE
        useEdges = true;
        useFaces = true;
    }
    else if (load < 1.2f)
    {
        // MID MODE
        useEdges = true;
        useFaces = false;
    }
    else
    {
        // LIGHT MODE
        useEdges = false;
        useFaces = false;
    }

    // -------- MOTION (always on) --------
    if (cfg.useMotion)
    {
        cv::Mat diff;
        cv::absdiff(gray, prevGray, diff);

        // cheaper than Gaussian
        cv::blur(diff, diff, cv::Size(5, 5));

        r.motion = normMean(diff);
    }

    // -------- EDGES (conditional) --------
    if (cfg.useEdges && useEdges)
    {
        cv::Mat gradX, gradY, mag;

        cv::Sobel(gray, gradX, CV_32F, 1, 0);
        cv::Sobel(gray, gradY, CV_32F, 0, 1);
        cv::magnitude(gradX, gradY, mag);

        r.edges = std::tanh(normMean(mag) * 2.5f);
    }

    // -------- FACES (conditional + throttled) --------
    if (cfg.useFaces && useFaces && !faceCascade.empty())
    {
        static int counter = 0;
        counter++;

        // throttle heavily (expensive)
        if (counter % 15 == 0)
        {
            cv::Mat small;
            cv::resize(gray, small, cv::Size(w / 2, h / 2));

            std::vector<cv::Rect> faces;
            faceCascade.detectMultiScale(small, faces);

            float score = 0.0f;

            for (auto& f : faces)
            {
                f.x *= 2;
                f.y *= 2;
                f.width *= 2;
                f.height *= 2;

                float area = (f.width * f.height) / float(w * h);
                score += area;
            }

            r.faces = std::tanh(score * 3.0f);
        }
    }

    // -------- GLOBAL FUSION --------
    float raw =
        0.5f * r.motion +
        0.3f * r.edges +
        0.2f * r.faces;

    // smoothing (prevents jitter)
    r.global = 0.85f * prevGlobal + 0.15f * raw;
    prevGlobal = r.global;

    r.global = std::clamp(r.global, 0.0f, 1.0f);

    r.confidence = 1.0f - std::abs(r.global - raw);

    return r;
}