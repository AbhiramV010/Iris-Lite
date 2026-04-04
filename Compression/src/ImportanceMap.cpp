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
    bg = cv::createBackgroundSubtractorMOG2(
        200,   // history
        16,    // varThreshold
        false  // detectShadows
    );

    // Preallocate buffers
    gray_.create(h, w, CV_8U);
    motion_.create(h, w, CV_32F);
    edges_.create(h, w, CV_32F);
    contrast_.create(h, w, CV_32F);
    centerBias_.create(h, w, CV_32F);
    tempFloat_.create(h, w, CV_32F);

    // Precompute center bias once
    float cx = w * 0.5f;
    float cy = h * 0.5f;

    for (int y = 0; y < h; y++)
    {
        for (int x = 0; x < w; x++)
        {
            float dx = (x - cx) / cx;
            float dy = (y - cy) / cy;
            float dist = std::sqrt(dx * dx + dy * dy);
            centerBias_.at<float>(y, x) = std::max(0.0f, 1.0f - dist);
        }
    }
    centerBias_ = normalize(centerBias_);

    logInfo("ImportanceMap optimized and initialized with MOG2 background subtractor");
}

ImportanceMap ImportanceMapGenerator::compute(const cv::Mat& smallFrame,
    const ImportanceMap& faceHeatmap)
{
    // Convert to gray (reuse buffer)
    cv::cvtColor(smallFrame, gray_, cv::COLOR_BGR2GRAY);

    // Motion
    ImportanceMap motion = useMotion ? computeMotion(gray_) : cv::Mat::zeros(h, w, CV_32F);

    // Edges
    ImportanceMap edges = useEdges ? computeEdges(gray_) : cv::Mat::zeros(h, w, CV_32F);

    // Contrast
    ImportanceMap contrast = computeContrast(gray_);

    // Center bias (precomputed)
    ImportanceMap center = centerBias_;

    // Weighted combination
    ImportanceMap combined =
        0.4f * motion +
        0.3f * edges +
        0.2f * contrast +
        0.1f * center;

    // Add face heatmap if enabled
    if (useFaces && !faceHeatmap.empty())
        combined += 0.5f * faceHeatmap;

    // Normalize to 0–1
    combined = normalize(combined);

    // Temporal smoothing (EMA)
    if (hasPrev)
        combined = 0.7f * prevImportance + 0.3f * combined;

    prevImportance = combined.clone();
    hasPrev = true;

    return combined;
}

ImportanceMap ImportanceMapGenerator::computeMotion(const cv::Mat& gray)
{
    cv::Mat fgMask;
    bg->apply(gray, fgMask, 0.01);

    fgMask.convertTo(motion_, CV_32F, 1.0f / 255.0f);
    return motion_;
}

ImportanceMap ImportanceMapGenerator::computeEdges(const cv::Mat& gray)
{
    cv::Mat gx, gy;

    // Scharr for better edges
    cv::Scharr(gray, gx, CV_32F, 1, 0);
    cv::Scharr(gray, gy, CV_32F, 0, 1);

    cv::magnitude(gx, gy, edges_);

    return normalize(edges_);
}

ImportanceMap ImportanceMapGenerator::computeContrast(const cv::Mat& gray)
{
    // Cheaper, smoother contrast: blur + absdiff
    cv::GaussianBlur(gray, tempFloat_, cv::Size(3, 3), 0);
    cv::absdiff(gray, tempFloat_, contrast_);
    contrast_.convertTo(contrast_, CV_32F, 1.0 / 255.0);

    return normalize(contrast_);
}

ImportanceMap ImportanceMapGenerator::computeCenterBias()
{
    // Just return the precomputed bias
    return centerBias_;
}

ImportanceMap ImportanceMapGenerator::normalize(const ImportanceMap& m)
{
    double minVal, maxVal;
    cv::minMaxLoc(m, &minVal, &maxVal);

    if (maxVal - minVal < 1e-6)
        return cv::Mat::zeros(m.size(), CV_32F);

    cv::Mat out;
    m.convertTo(out, CV_32F, 1.0 / (maxVal - minVal), -minVal / (maxVal - minVal));
    return out;
}
