#include "ImportanceMap.hpp"
#include "Utils.hpp"
#include <opencv2/imgproc.hpp>
#include <opencv2/video.hpp>
#include <algorithm>
#include <cmath>
#include "logging.hpp"

ImportanceMapGenerator::ImportanceMapGenerator(int width, int height,
    bool useFaces_, bool useMotion_, bool useEdges_)
    : w(width), h(height),
    useFaces(useFaces_), useMotion(useMotion_), useEdges(useEdges_)
{
    // Initialize MOG2 background subtraction model
    // History=200: learns from 200 frames before detecting foreground
    // VarThreshold=16: detect pixels >16 standard deviations from model as foreground
    // detectShadows=false: ignore shadow pixels (reduce false positives)
    bg = cv::createBackgroundSubtractorMOG2(200, 16, false);

    // Preallocate working buffers to avoid allocation overhead each frame
    gray_.create(h, w, CV_8U);
    motion_.create(h, w, CV_32F);
    edges_.create(h, w, CV_32F);
    contrast_.create(h, w, CV_32F);
    centerBias_.create(h, w, CV_32F);
    tempFloat_.create(h, w, CV_32F);

    // Precompute center bias (Gaussian weighting toward frame center)
    // Only computed once at initialization, reused every frame
    float cx = w * 0.5f;
    float cy = h * 0.5f;

    for (int y = 0; y < h; y++)
    {
        for (int x = 0; x < w; x++)
        {
            // Normalized distance from center (0 at center, ~1.4 at corners)
            float dx = (x - cx) / cx;
            float dy = (y - cy) / cy;
            float dist = std::sqrt(dx * dx + dy * dy);

            // Gaussian bias: 1.0 at center, decays toward edges
            centerBias_.at<float>(y, x) = std::max(0.0f, 1.0f - dist);
        }
    }
    centerBias_ = normalize(centerBias_);

    logInfo("ImportanceMapGenerator initialized with adaptive temporal smoothing");
}

ImportanceMap ImportanceMapGenerator::compute(const cv::Mat& smallFrame,
    const ImportanceMap& faceHeatmap)
{
    // Convert input frame to grayscale for analysis
    cv::cvtColor(smallFrame, gray_, cv::COLOR_BGR2GRAY);

    // Compute motion importance (foreground pixels from background subtraction)
    ImportanceMap motion = useMotion ? computeMotion(gray_) : cv::Mat::zeros(h, w, CV_32F);

    // Compute edge importance (object boundaries and texture)
    ImportanceMap edges = useEdges ? computeEdges(gray_) : cv::Mat::zeros(h, w, CV_32F);

    // Compute contrast importance (local pixel variation)
    ImportanceMap contrast = computeContrast(gray_);

    // Retrieve precomputed center bias
    ImportanceMap center = centerBias_;

    // Combine all features with weighted sum (40% motion, 30% edges, 20% contrast, 10% center)
    ImportanceMap combined =
        0.4f * motion +
        0.3f * edges +
        0.2f * contrast +
        0.1f * center;

    // Add face heatmap if face detection enabled and faces detected
    if (useFaces && !faceHeatmap.empty())
        combined += 0.5f * faceHeatmap;  // Additional 50% boost for face regions

    // Normalize combined map to [0, 1] range
    combined = normalize(combined);

    // Temporal filtering with adaptive smoothing based on activity level
    // Calculate mean importance as proxy for scene activity
    cv::Scalar meanVal = cv::mean(combined);
    float activity_level = static_cast<float>(meanVal[0]);

    if (hasPrev)
    {
        // Adaptive EMA weight: more active scenes use lighter smoothing (more responsive)
        // High activity (0.8-1.0): alpha=0.5 (50/50 blend, responsive)
        // Low activity (0.0-0.2): alpha=0.7 (70/30 blend, smoother)
        float alpha = 0.7f - (activity_level * 0.2f);
        combined = alpha * prevImportance + (1.0f - alpha) * combined;
    }

    // Store for next frame's temporal smoothing
    prevImportance = combined.clone();
    hasPrev = true;

    return combined;
}

ImportanceMap ImportanceMapGenerator::computeMotion(const cv::Mat& gray)
{
    // Apply MOG2 background subtraction to detect moving objects
    cv::Mat fgMask;
    bg->apply(gray, fgMask, 0.01);  // 0.01 = learning rate (small = slow adaptation)

    // Normalize foreground mask to [0, 1] float for importance weighting
    fgMask.convertTo(motion_, CV_32F, 1.0f / 255.0f);
    return motion_;
}

ImportanceMap ImportanceMapGenerator::computeEdges(const cv::Mat& gray)
{
    // Compute image gradients using Scharr operator (better diagonal edge detection than Sobel)
    cv::Mat gx, gy;
    cv::Scharr(gray, gx, CV_32F, 1, 0);  // Horizontal edges
    cv::Scharr(gray, gy, CV_32F, 0, 1);  // Vertical edges

    // Combine gradient components into edge magnitude
    cv::magnitude(gx, gy, edges_);

    // Normalize to [0, 1] range
    return normalize(edges_);
}

ImportanceMap ImportanceMapGenerator::computeContrast(const cv::Mat& gray)
{
    // Compute local intensity variation as measure of texture/detail
    // Method: blur frame, then compute difference (removes noise while preserving texture)
    cv::GaussianBlur(gray, tempFloat_, cv::Size(3, 3), 0);
    cv::absdiff(gray, tempFloat_, contrast_);

    // Convert to float and normalize to [0, 1]
    contrast_.convertTo(contrast_, CV_32F, 1.0 / 255.0);

    return normalize(contrast_);
}

ImportanceMap ImportanceMapGenerator::normalize(const ImportanceMap& m)
{
    // Find min and max values in map
    double minVal, maxVal;
    cv::minMaxLoc(m, &minVal, &maxVal);

    // Handle edge case: entire map is uniform
    if (maxVal - minVal < 1e-6)
        return cv::Mat::zeros(m.size(), CV_32F);

    // Rescale to [0, 1]: (x - min) / (max - min)
    cv::Mat out;
    m.convertTo(out, CV_32F, 1.0 / (maxVal - minVal), -minVal / (maxVal - minVal));
    return out;
}