#include "ImportanceEngine.hpp"
#include "logging.hpp"

#include <opencv2/imgproc.hpp>
#include <opencv2/objdetect.hpp>

ImportanceEngine::ImportanceEngine(int width, int height, const Config& cfg_)
    : w(width), h(height), cfg(cfg_)
{
    logInfo("ImportanceEngine (real CV) initialized");

    if (cfg.useFaces)
    {
        faceCascade.load("data/haarcascade_frontalface_default.xml");
    }
}

ImportanceEngine::~ImportanceEngine() {}

ImportanceSignal ImportanceEngine::analyze(const cv::Mat& frame, const cv::Mat& prev)
{
    ImportanceSignal r;

    if (frame.empty())
        return r;

    cv::Mat resized, gray;
    cv::resize(frame, resized, cv::Size(w, h));
    cv::cvtColor(resized, gray, cv::COLOR_BGR2GRAY);

    // ---------------- MOTION ----------------
    if (cfg.useMotion && !prev.empty())
    {
        cv::Mat prevResized, prevGray;
        cv::resize(prev, prevResized, cv::Size(w, h));
        cv::cvtColor(prevResized, prevGray, cv::COLOR_BGR2GRAY);

        cv::Mat diff;
        cv::absdiff(gray, prevGray, diff);

        r.motion = static_cast<float>(cv::mean(diff)[0]) / 255.0f;
    }

    // ---------------- EDGES ----------------
    if (cfg.useEdges)
    {
        cv::Mat gradX, gradY;
        cv::Sobel(gray, gradX, CV_32F, 1, 0);
        cv::Sobel(gray, gradY, CV_32F, 0, 1);

        cv::Mat mag;
        cv::magnitude(gradX, gradY, mag);

        r.edges = static_cast<float>(cv::mean(mag)[0]) / 255.0f;
    }

    // ---------------- FACES ----------------
    if (cfg.useFaces && !faceCascade.empty())
    {
        std::vector<cv::Rect> faces;
        faceCascade.detectMultiScale(gray, faces);

        r.faces = faces.empty() ? 0.0f : 1.0f;
    }

    // ---------------- GLOBAL ----------------
    r.global =
        0.5f * r.motion +
        0.4f * r.edges +
        0.1f * r.faces;

    return r;
}