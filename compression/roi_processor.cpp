#include "roi_processor.hpp"
#include <opencv2/opencv.hpp>

// This file handles the region-adaptive part of the compression.
// The idea is to keep moving/important areas sharp, and simplify the background.
// To keep things light on the Raspberry Pi, we don't analyze every single frame.
// Instead, we reuse the last motion mask for a few frames in between.

cv::Mat ROIProcessor::processFrame(const cv::Mat& frame) {
    static cv::Mat prevFrame;
    static cv::Mat lastMask;
    static int frameCounter = 0;

    // How often we actually compute motion (every N frames)
    const int analysisInterval = 3;

    // First frame: nothing to compare to yet
    if (prevFrame.empty()) {
        prevFrame = frame.clone();
        lastMask = cv::Mat::zeros(frame.size(), CV_8UC1);
        return frame.clone();
    }

    cv::Mat motionMask;

    // Only analyze motion every few frames
    if (frameCounter % analysisInterval == 0) {
        cv::Mat grayPrev, grayCurr, diff;

        cv::cvtColor(prevFrame, grayPrev, cv::COLOR_BGR2GRAY);
        cv::cvtColor(frame, grayCurr, cv::COLOR_BGR2GRAY);

        cv::absdiff(grayPrev, grayCurr, diff);
        cv::threshold(diff, motionMask, 25, 255, cv::THRESH_BINARY);

        // Clean up noise a bit
        cv::erode(motionMask, motionMask, cv::Mat(), cv::Point(-1, -1), 1);
        cv::dilate(motionMask, motionMask, cv::Mat(), cv::Point(-1, -1), 2);

        lastMask = motionMask.clone();
        prevFrame = frame.clone();
    }
    else {
        // Reuse the last mask to save CPU
        motionMask = lastMask.clone();
    }

    frameCounter++;

    // Convert mask to 3 channels for blending
    cv::Mat mask3;
    cv::cvtColor(motionMask, mask3, cv::COLOR_GRAY2BGR);
    mask3.convertTo(mask3, CV_32F, 1.0 / 255.0);

    // Background simplification: downscale → upscale
    cv::Mat small, blurredBackground;
    cv::resize(frame, small, cv::Size(), 0.33, 0.33, cv::INTER_AREA);
    cv::resize(small, blurredBackground, frame.size(), 0, 0, cv::INTER_LINEAR);

    // Convert to float for blending
    cv::Mat frameF, bgF;
    frame.convertTo(frameF, CV_32F);
    blurredBackground.convertTo(bgF, CV_32F);

    // Blend: motion areas stay sharp, background gets simplified
    cv::Mat blended = frameF.mul(mask3) + bgF.mul(1.0 - mask3);

    blended.convertTo(blended, CV_8U);
    return blended;
}
