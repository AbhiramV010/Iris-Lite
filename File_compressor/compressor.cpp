#include "compressor.hpp"
#include <opencv2/opencv.hpp>
#include <iostream>

Compressor::Compressor() {}

void Compressor::startNewClip(const std::string& filename) {
    int fourcc = cv::VideoWriter::fourcc('H', '2', '6', '4');
    writer.open(filename, fourcc, 20.0, cv::Size(640, 480));
    isRecording = true;
}

void Compressor::endClip() {
    if (isRecording) {
        writer.release();
        isRecording = false;
    }
}

void Compressor::addFrame(const cv::Mat& frame, const cv::Mat& fgMask) {
    if (!isRecording) return;

    cv::Mat processed = processFrame(frame, fgMask);
    writer.write(processed);
}

cv::Mat Compressor::processFrame(const cv::Mat& frame, const cv::Mat& fgMask) {
    cv::Mat fg = frame.clone();
    cv::Mat bg = frame.clone();

    enhanceForeground(fg);
    compressBackground(bg);

    cv::Mat merged = frame.clone();
    for (int y = 0; y < frame.rows; y++) {
        for (int x = 0; x < frame.cols; x++) {
            if (fgMask.at<uchar>(y, x) > 0)
                merged.at<cv::Vec3b>(y, x) = fg.at<cv::Vec3b>(y, x);
            else
                merged.at<cv::Vec3b>(y, x) = bg.at<cv::Vec3b>(y, x);
        }
    }

    boostVibrancy(merged);
    return merged;
}

void Compressor::enhanceForeground(cv::Mat& fg) {
    cv::Mat sharp;
    cv::GaussianBlur(fg, sharp, cv::Size(0, 0), 1.0);
    cv::addWeighted(fg, 1.5, sharp, -0.5, 0, fg);
}

void Compressor::compressBackground(cv::Mat& bg) {
    cv::GaussianBlur(bg, bg, cv::Size(5, 5), 1.2);
}

void Compressor::boostVibrancy(cv::Mat& img) {
    cv::Mat hsv;
    cv::cvtColor(img, hsv, cv::COLOR_BGR2HSV);

    for (int y = 0; y < hsv.rows; y++) {
        for (int x = 0; x < hsv.cols; x++) {
            auto& p = hsv.at<cv::Vec3b>(y, x);
            p[1] = cv::saturate_cast<uchar>(p[1] * 1.15);
        }
    }

    cv::cvtColor(hsv, img, cv::COLOR_HSV2BGR);
}
