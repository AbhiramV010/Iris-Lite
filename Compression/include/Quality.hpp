#pragma once
#include <opencv2/core.hpp>

class Quality
{
public:
    // Compute SSIM between original and compressed frames (range 0-1, 1.0 = identical)
    static double computeSSIM(const cv::Mat& original, const cv::Mat& compressed);

    // Compute PSNR in dB (higher = better, >40dB = transparent compression)
    static double computePSNR(const cv::Mat& original, const cv::Mat& compressed);

private:
    // Helper to create 2D Gaussian kernel for SSIM computation
    static cv::Mat gaussianKernel2D(int size, double sigma);
};