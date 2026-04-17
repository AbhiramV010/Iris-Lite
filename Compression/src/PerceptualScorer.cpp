#include "PerceptualScorer.hpp"
#include "logging.hpp"
#include <algorithm>
#include <cmath>

PerceptualScorer::PerceptualScorer(int width, int height)
    : w(width), h(height),
    lastMotionScore(0.0f),
    lastEdgeScore(0.0f),
    lastStabilityScore(0.5f)
{
    prevGray = cv::Mat::zeros(height, width, CV_8U);
    logInfo("PerceptualScorer initialized (SAFE MODE)");
}

PerceptualScorer::~PerceptualScorer() {}

static inline cv::Mat ensureSizeGray(const cv::Mat& input, cv::Size target)
{
    cv::Mat gray;

    if (input.channels() == 3)
        cv::cvtColor(input, gray, cv::COLOR_BGR2GRAY);
    else
        gray = input.clone();

    if (gray.size() != target)
        cv::resize(gray, gray, target);

    return gray;
}

float PerceptualScorer::scoreFrame(const cv::Mat& frame, const cv::Mat& prevFrame)
{
    if (frame.empty())
        return 0.0f;

    cv::Size target(w, h);

    cv::Mat gray = ensureSizeGray(frame, target);

    // ensure prevGray is valid size
    if (prevGray.empty() || prevGray.size() != target)
        prevGray = cv::Mat::zeros(target, CV_8U);

    float motionScore = computeMotion(gray, prevGray);
    float edgeScore = computeEdges(gray);
    float stabilityScore = computeTemporalStability(gray, prevGray);

    gray.copyTo(prevGray);

    lastMotionScore = motionScore;
    lastEdgeScore = edgeScore;
    lastStabilityScore = stabilityScore;

    float combined =
        0.5f * motionScore +
        0.3f * edgeScore +
        0.2f * stabilityScore;

    return std::clamp(combined, 0.0f, 1.0f);
}

float PerceptualScorer::computeMotion(const cv::Mat& gray, const cv::Mat& prev)
{
    if (gray.size() != prev.size())
        return 0.0f;

    cv::Mat diff;
    cv::absdiff(gray, prev, diff);

    cv::Mat thresh;
    cv::threshold(diff, thresh, 30, 1, cv::THRESH_BINARY);

    float changed = (float)cv::countNonZero(thresh);
    float total = (float)(gray.rows * gray.cols);

    return std::min(1.0f, (changed / total) * 10.0f);
}

float PerceptualScorer::computeEdges(const cv::Mat& gray)
{
    cv::Mat sobelX, sobelY;
    cv::Sobel(gray, sobelX, CV_32F, 1, 0, 3);
    cv::Sobel(gray, sobelY, CV_32F, 0, 1, 3);

    cv::Mat mag;
    cv::magnitude(sobelX, sobelY, mag);

    return std::min(1.0f, (float)cv::mean(mag)[0] / 50.0f);
}

float PerceptualScorer::computeTemporalStability(const cv::Mat& gray, const cv::Mat& prev)
{
    if (prev.empty() || gray.size() != prev.size())
        return 0.5f;

    cv::Mat diff;
    cv::absdiff(gray, prev, diff);

    float mse = (float)cv::mean(diff)[0];

    return std::clamp(1.0f - mse / 255.0f, 0.0f, 1.0f);
}