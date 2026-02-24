#pragma once
#include <opencv2/opencv.hpp>
#include <string>

class Compressor {
public:
    Compressor();
    void startNewClip(const std::string& filename);   // Start recording
    void addFrame(const cv::Mat& frame, const cv::Mat& fgMask); // Add processed frame
    void endClip();                                   // Stop recording

private:
    cv::VideoWriter writer;
    bool isRecording = false;

    // Internal processing
    cv::Mat processFrame(const cv::Mat& frame, const cv::Mat& fgMask);
    void enhanceForeground(cv::Mat& fg);   // Sharpen intruder/object
    void compressBackground(cv::Mat& bg);  // Blur grass
    void boostVibrancy(cv::Mat& img);      // Make video look crisp
};
