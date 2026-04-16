#include "PerceptualScorer.hpp"
#include "logging.hpp"
#include <algorithm>
#include <cmath>

PerceptualScorer::PerceptualScorer(int width, int height)
    : w(width), h(height),
    lastMotionScore(0.0f), lastEdgeScore(0.0f), lastStabilityScore(0.5f)
{
    prevGray.create(height, width, CV_8U);
    workGray.create(height, width, CV_8U);
    logInfo("PerceptualScorer initialized");
}

PerceptualScorer::~PerceptualScorer()
{
}

float PerceptualScorer::scoreFrame(const cv::Mat& frame, const cv::Mat& prevFrame)
{
    if (frame.empty())
        return 0.0f;

    cv::Mat gray;
    cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);

    float motionScore = 0.0f;
    if (!prevGray.empty() && !prevFrame.empty())
    {
        motionScore = computeMotion(gray, prevGray);
    }

    float edgeScore = computeEdges(gray);
    float stabilityScore = computeTemporalStability(gray, prevGray);

    gray.copyTo(prevGray);

    lastMotionScore = motionScore;
    lastEdgeScore = edgeScore;
    lastStabilityScore = stabilityScore;

    float combined = 0.5f * motionScore + 0.3f * edgeScore + 0.2f * stabilityScore;
    return std::min(1.0f, std::max(0.0f, combined));
}

float PerceptualScorer::computeMotion(const cv::Mat& gray, const cv::Mat& prevGray)
{
    cv::Mat diff;
    cv::absdiff(gray, prevGray, diff);

    cv::Mat thresh;
    cv::threshold(diff, thresh, 30, 1, cv::THRESH_BINARY);

    float pixelsChanged = cv::countNonZero(thresh);
    float totalPixels = w * h;

    return std::min(1.0f, pixelsChanged / totalPixels * 10.0f);
}

float PerceptualScorer::computeEdges(const cv::Mat& gray)
{
    cv::Mat sobelX, sobelY;
    cv::Sobel(gray, sobelX, CV_32F, 1, 0, 3);
    cv::Sobel(gray, sobelY, CV_32F, 0, 1, 3);

    cv::Mat magnitude;
    cv::magnitude(sobelX, sobelY, magnitude);

    double meanMag = cv::mean(magnitude)[0];

    return std::min(1.0f, static_cast<float>(meanMag) / 50.0f);
}

float PerceptualScorer::computeTemporalStability(const cv::Mat& gray, const cv::Mat& prevGray)
{
    if (prevGray.empty())
        return 0.5f;

    cv::Mat diff;
    cv::absdiff(gray, prevGray, diff);
    double mse = cv::mean(diff)[0];

    return std::min(1.0f, 1.0f - static_cast<float>(mse) / 255.0f);
}