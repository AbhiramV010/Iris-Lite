#include "compressor.hpp"
#include "roi_processor.hpp"
#include "utils.hpp"
#include <opencv2/opencv.hpp>
#include <filesystem>
#include <iostream>

// This file handles the actual video compression step.
// The idea is:
// 1. Read the input video frame-by-frame.
// 2. Run region-adaptive processing on each frame.
// 3. Keep track of how much motion is happening overall.
// 4. Use that motion score to decide if the clip is "useful" or not.
// 5. Save the compressed video, and clean up the raw file afterward.

bool Compressor::compressVideo(const std::string& inputPath, const std::string& outputPath) {
    cv::VideoCapture cap(inputPath);
    if (!cap.isOpened()) {
        std::cout << "Could not open input video: " << inputPath << "\n";
        return false;
    }

    int width = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH));
    int height = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT));
    double fps = cap.get(cv::CAP_PROP_FPS);

    // AVC1 is a common H.264-compatible codec.
    cv::VideoWriter writer(
        outputPath,
        cv::VideoWriter::fourcc('a', 'v', 'c', '1'),
        fps,
        cv::Size(width, height)
    );

    if (!writer.isOpened()) {
        std::cout << "Could not open output writer: " << outputPath << "\n";
        return false;
    }

    ROIProcessor roi;

    int totalMotionScore = 0;
    int frameCount = 0;

    cv::Mat frame;
    while (cap.read(frame)) {
        // Process the frame (ROI, background simplification, etc.)
        cv::Mat processed = roi.processFrame(frame);

        // Compute motion score from the mask stored inside ROIProcessor
        // Since ROIProcessor hides the mask internally, we re-run a simple diff here.
        // This keeps the code cleaner and avoids exposing too much internal state.
        cv::Mat grayPrev, grayCurr, diff, mask;
        if (frameCount > 0) {
            cv::cvtColor(prevFrame_, grayPrev, cv::COLOR_BGR2GRAY);
            cv::cvtColor(frame, grayCurr, cv::COLOR_BGR2GRAY);
            cv::absdiff(grayPrev, grayCurr, diff);
            cv::threshold(diff, mask, 25, 255, cv::THRESH_BINARY);
            totalMotionScore += cv::countNonZero(mask);
        }

        prevFrame_ = frame.clone();
        frameCount++;

        writer.write(processed);
    }

    // Compute average motion score for the whole clip
    int avgMotion = (frameCount > 0) ? totalMotionScore / frameCount : 0;

    std::cout << "Finished compressing: " << inputPath << "\n";
    std::cout << "Average motion score: " << avgMotion << "\n";

    // Decide if the clip is useful or not
    // This threshold is intentionally simple. You can tune it later.
    if (avgMotion < 5000) {
        std::cout << "Low activity detected. Marking clip as low-value.\n";
    }
    else {
        std::cout << "Higher activity detected. Marking clip as useful.\n";
    }

    // Privacy: delete the raw file after successful compression
    try {
        std::filesystem::remove(inputPath);
        std::cout << "Deleted raw file: " << inputPath << "\n";
    }
    catch (...) {
        std::cout << "Warning: could not delete raw file.\n";
    }

    return true;
}
