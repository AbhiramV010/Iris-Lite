#include "FaceDetector.hpp"
#include "Utils.hpp"
#include <opencv2/imgproc.hpp>
#include "logging.hpp"
#include <cmath>

FaceDetector::FaceDetector(int width, int height, bool enabled_)
    : w(width), h(height), enabled(enabled_)
{
    logInfo("FaceDetector initializing Haar Cascade classifier");

    // Load pretrained Haar Cascade for frontal face detection
    std::string cascadePath =
        "C:/Users/shrey/source/repos/Compressor/data/haarcascade_frontalface_default.xml";

    if (!faceCascade.load(cascadePath))
    {
        logError("Unable to load Haar Cascade from: " + cascadePath);
        logWarn("Face detection disabled; system continues without face importance boost");
        enabled = false;
    }
}

ImportanceMap FaceDetector::detect(const cv::Mat& smallFrame)
{
    // If face detection disabled, return empty heatmap
    if (!enabled)
        return cv::Mat::zeros(h, w, CV_32F);

    // Preprocess frame for better detection: convert to grayscale and equalize histogram
    cv::Mat gray;
    cv::cvtColor(smallFrame, gray, cv::COLOR_BGR2GRAY);
    cv::equalizeHist(gray, gray);  // Normalize contrast to improve detection robustness

    // Run cascade detector at multiple scales (1.1 = 10% scale increment per level)
    std::vector<cv::Rect> faces;
    faceCascade.detectMultiScale(
        gray,
        faces,
        1.1,       // Scale increment factor
        3,         // Min neighbors (require 3 overlapping detections for valid face)
        0,         // Flags (0 = default)
        cv::Size(20, 20)  // Minimum face size (20×20 pixels)
    );

    // If no faces detected, return empty heatmap
    if (faces.empty())
        return cv::Mat::zeros(h, w, CV_32F);

    // Calculate confidence scores for each detected face
    // Faces on frame edges get lower confidence (typically less important for ID)
    std::vector<double> confidences;
    int centerX = w / 2;
    int centerY = h / 2;

    for (const auto& face : faces)
    {
        // Calculate face center position
        float faceCenterX = face.x + face.width / 2.0f;
        float faceCenterY = face.y + face.height / 2.0f;

        // Normalized distance from frame center (0 = center, ~1.4 = corners)
        float dx = (faceCenterX - centerX) / (w / 2.0f);
        float dy = (faceCenterY - centerY) / (h / 2.0f);
        float edgeDistance = std::sqrt(dx * dx + dy * dy);

        // Confidence decreases if face is on frame edge
        // Center face: confidence ≈ 1.0, Edge face: confidence ≈ 0.6
        double confidence = std::max(0.6, 1.0 - edgeDistance * 0.2);
        confidences.push_back(confidence);
    }

    // Generate heatmap with confidence weighting
    return generateHeatmapWithConfidence(faces, confidences);
}

ImportanceMap FaceDetector::generateHeatmapWithConfidence(
    const std::vector<cv::Rect>& faces,
    const std::vector<double>& confidences)
{
    // Initialize empty heatmap
    ImportanceMap heat = cv::Mat::zeros(h, w, CV_32F);

    // For each detected face, add Gaussian bump to heatmap
    for (size_t i = 0; i < faces.size(); i++)
    {
        const auto& face = faces[i];
        double confidence = confidences[i];  // 0.6-1.0 (lower for edge faces)

        // Calculate face center and radius for Gaussian
        cv::Point center(face.x + face.width / 2, face.y + face.height / 2);
        float radius = std::max(face.width, face.height) * 0.75f;

        // Generate Gaussian-weighted contribution for this face
        for (int y = 0; y < h; y++)
        {
            for (int x = 0; x < w; x++)
            {
                // Euclidean distance from face center
                float dx = x - center.x;
                float dy = y - center.y;
                float dist = std::sqrt(dx * dx + dy * dy);

                // Gaussian function: peak at face center, decays with distance
                // Weighed by confidence (edge faces contribute less)
                float value = confidence * std::exp(-(dist * dist) / (2 * radius * radius));

                // Take maximum if multiple faces overlap
                heat.at<float>(y, x) = std::max(heat.at<float>(y, x), value);
            }
        }
    }

    // Normalize heatmap to [0, 1] range
    double minVal, maxVal;
    cv::minMaxLoc(heat, &minVal, &maxVal);

    if (maxVal > 1e-6)
        heat /= maxVal;

    return heat;
}