#include "ImportanceMap.hpp"
#include "Utils.hpp"
#include <opencv2/imgproc.hpp>
#include <opencv2/video.hpp>   // MOG2 lives here
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

    logInfo("ImportanceMap initialized with MOG2 background subtractor");
}

ImportanceMap ImportanceMapGenerator::compute(const cv::Mat& smallFrame,
    const ImportanceMap& faceHeatmap)
{
    cv::Mat gray;
    cv::cvtColor(smallFrame, gray, cv::COLOR_BGR2GRAY);

    ImportanceMap motion = useMotion ? computeMotion(gray) : cv::Mat::zeros(h, w, CV_32F);
    ImportanceMap edges = useEdges ? computeEdges(gray) : cv::Mat::zeros(h, w, CV_32F);
    ImportanceMap contrast = computeContrast(gray);
    ImportanceMap center = computeCenterBias();

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

    cv::Mat floatMask;
    fgMask.convertTo(floatMask, CV_32F, 1.0f / 255.0f);

    return floatMask;
}

ImportanceMap ImportanceMapGenerator::computeEdges(const cv::Mat& gray)
{
    cv::Mat sobelX, sobelY, mag;

    cv::Sobel(gray, sobelX, CV_32F, 1, 0, 3);
    cv::Sobel(gray, sobelY, CV_32F, 0, 1, 3);

    cv::magnitude(sobelX, sobelY, mag);

    return normalize(mag);
}

ImportanceMap ImportanceMapGenerator::computeContrast(const cv::Mat& gray)
{
    cv::Mat lap;
    cv::Laplacian(gray, lap, CV_32F);

    cv::Mat absLap = cv::abs(lap);
    return normalize(absLap);
}

ImportanceMap ImportanceMapGenerator::computeCenterBias()
{
    cv::Mat bias(h, w, CV_32F);

    float cx = w * 0.5f;
    float cy = h * 0.5f;

    for (int y = 0; y < h; y++)
    {
        for (int x = 0; x < w; x++)
        {
            float dx = (x - cx) / cx;
            float dy = (y - cy) / cy;
            float dist = std::sqrt(dx * dx + dy * dy);

            // Clamp to avoid negative values
            bias.at<float>(y, x) = std::max(0.0f, 1.0f - dist);
        }
    }

    return normalize(bias);
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
