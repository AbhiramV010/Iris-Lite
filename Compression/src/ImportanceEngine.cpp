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
        faceCascade.load("data/haarcascade_frontalface_default.xml");
}

ImportanceEngine::~ImportanceEngine() {}

static float normalizeMatMean(const cv::Mat& m)
{
    return static_cast<float>(cv::mean(m)[0]) / 255.0f;
}

ImportanceSignal ImportanceEngine::analyze(const cv::Mat& frame, const cv::Mat& prev)
{
    ImportanceSignal r{};

    if (frame.empty())
        return r;

    cv::Mat resized, gray;
    cv::resize(frame, resized, cv::Size(w, h));
    cv::cvtColor(resized, gray, cv::COLOR_BGR2GRAY);

    cv::Mat prevResized, prevGray;
    const cv::Mat& safePrev = prev.empty() ? frame : prev;

    cv::resize(safePrev, prevResized, cv::Size(w, h));
    cv::cvtColor(prevResized, prevGray, cv::COLOR_BGR2GRAY);

    // -------- MOTION --------
    if (cfg.useMotion)
    {
        cv::Mat diff;
        cv::absdiff(gray, prevGray, diff);
        cv::GaussianBlur(diff, diff, cv::Size(5, 5), 0);
        r.motion = normalizeMatMean(diff);
    }

    // -------- EDGES --------
    if (cfg.useEdges)
    {
        cv::Mat gradX, gradY, mag;
        cv::Sobel(gray, gradX, CV_32F, 1, 0);
        cv::Sobel(gray, gradY, CV_32F, 0, 1);
        cv::magnitude(gradX, gradY, mag);

        r.edges = std::tanh(normalizeMatMean(mag) * 2.5f);
    }

    // -------- FACES (OPTIMIZED) --------
    if (cfg.useFaces && !faceCascade.empty())
    {
        static int counter = 0;
        counter++;

        if (counter % 10 == 0)
        {
            cv::Mat small;
            cv::resize(gray, small, cv::Size(w / 2, h / 2));

            std::vector<cv::Rect> faces;
            faceCascade.detectMultiScale(small, faces);

            float faceScore = 0.0f;

            for (auto& f : faces)
            {
                f.x *= 2;
                f.y *= 2;
                f.width *= 2;
                f.height *= 2;

                float area = (f.width * f.height) / float(w * h);
                faceScore += area;
            }

            r.faces = std::tanh(faceScore * 3.0f);
        }
    }

    // -------- GLOBAL --------
    float raw =
        0.45f * r.motion +
        0.35f * r.edges +
        0.20f * r.faces;

    r.global = 0.85f * prevGlobal + 0.15f * raw;
    prevGlobal = r.global;

    r.global = std::clamp(r.global, 0.0f, 1.0f);
    r.confidence = 1.0f - std::abs(r.global - raw);

    return r;
}