#pragma once

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <cstdint>

class PerceptualScorer
{
public:
    PerceptualScorer(int width, int height);
    ~PerceptualScorer();

    float scoreFrame(const cv::Mat& frame, const cv::Mat& prevFrame);

    float getMotionScore() const { return lastMotionScore; }
    float getEdgeScore() const { return lastEdgeScore; }
    float getTemporalStability() const { return lastStabilityScore; }

private:
    int w, h;

    cv::Mat prevGray;

    float lastMotionScore;
    float lastEdgeScore;
    float lastStabilityScore;

    float computeMotion(const cv::Mat& gray, const cv::Mat& prevGray);
    float computeEdges(const cv::Mat& gray);
    float computeTemporalStability(const cv::Mat& gray, const cv::Mat& prevGray);
};