#include "FrameProcessor.hpp"
#include "Utils.hpp"
#include <opencv2/imgproc.hpp>
#include "Config.hpp"
#include "logging.hpp"

FrameProcessor::FrameProcessor(int width, int height)
    : w(width), h(height)
{
    logInfo("FrameProcessor initialized for adaptive perceptual blending");

    // Preallocate working buffers to avoid per-frame allocation overhead
    smooth_.create(h, w, CV_8UC3);
    impResized_.create(h, w, CV_32F);
    imp3_.create(h, w, CV_32FC3);
    sharpF_.create(h, w, CV_32FC3);
    smoothF_.create(h, w, CV_32FC3);
    outF_.create(h, w, CV_32FC3);
    oneMinusImp_.create(h, w, CV_32FC3);
}

cv::Mat FrameProcessor::createSmoothBilateral(const cv::Mat& frame)
{
    // Bilateral filtering: smooths regions while preserving sharp boundaries (no halo artifacts)
    // Parameters: diameter=9 (neighbor region), sigmaColor/sigmaSpatial=75 (weighting strength)
    cv::bilateralFilter(frame, smooth_, 9, 75, 75);
    return smooth_;
}

cv::Mat FrameProcessor::process(const cv::Mat& fullFrame, const ImportanceMap& importance)
{
    // Step 1: Create two versions of the frame
    const cv::Mat& sharp = fullFrame;  // Original frame (preserve detail where important)
    const cv::Mat& smooth = createSmoothBilateral(fullFrame);  // Smoothed frame (compress where unimportant)

    // Step 2: Upscale importance map from analysis resolution to full frame resolution
    // Uses bilinear interpolation for smooth transitions between pixels
    cv::resize(importance, impResized_, fullFrame.size(), 0, 0, cv::INTER_LINEAR);

    // Step 3: Replicate single-channel importance map across 3 color channels (B, G, R)
    // This allows per-pixel weighting across all color channels uniformly
    cv::Mat channels[] = { impResized_, impResized_, impResized_ };
    cv::merge(channels, 3, imp3_);

    // Step 4: Convert source frames to floating-point for blending operations
    // Allows arithmetic operations without overflow/underflow
    sharp.convertTo(sharpF_, CV_32F);
    smooth.convertTo(smoothF_, CV_32F);

    // Step 5: Compute inverted importance map: (1 - importance)
    // Used for smooth regions: output = importance*sharp + (1-importance)*smooth
    cv::subtract(1.0f, imp3_, oneMinusImp_);

    // Step 6: Blend sharp and smooth versions based on importance
    // High importance (close to 1.0): output ≈ sharp (preserve detail)
    // Low importance (close to 0.0): output ≈ smooth (aggressive compression)
    // Formula: output = imp*sharp + (1-imp)*smooth
    cv::multiply(imp3_, sharpF_, outF_);
    cv::Mat tmp;
    cv::multiply(oneMinusImp_, smoothF_, tmp);
    outF_ += tmp;

    // Step 7: Convert back to 8-bit unsigned integer format for encoding
    // Clamps values to [0, 255] range and converts data type
    cv::Mat out;
    outF_.convertTo(out, CV_8U);
    return out;
}