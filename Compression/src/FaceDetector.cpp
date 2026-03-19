#include "FaceDetector.hpp"
#include "Utils.hpp"
#include <opencv2/imgproc.hpp>
#include "logging.hpp"

FaceDetector::FaceDetector(int width, int height, bool enabled_)
    : w(width), h(height), enabled(enabled_)
{
    logInfo("FaceDetector initialized");

    // Load cascade
    std::string cascadePath =
        "C:/Users/shrey/source/repos/Compressor/data/haarcascade_frontalface_default.xml";

    if (!faceCascade.load(cascadePath))
    {
        logError("Failed to load face cascade: " + cascadePath);
        enabled = false; // disable face detection safely
    }
}

ImportanceMap FaceDetector::detect(const cv::Mat& smallFrame)
{
    if (!enabled)
        return cv::Mat::zeros(h, w, CV_32F);

    cv::Mat gray;
    cv::cvtColor(smallFrame, gray, cv::COLOR_BGR2GRAY);
    cv::equalizeHist(gray, gray);

    std::vector<cv::Rect> faces;
    faceCascade.detectMultiScale(
        gray,
        faces,
        1.1,
        3,
        0,
        cv::Size(20, 20)
    );

    if (faces.empty())
        return cv::Mat::zeros(h, w, CV_32F);

    return generateHeatmap(faces);
}

ImportanceMap FaceDetector::generateHeatmap(const std::vector<cv::Rect>& faces)
{
    ImportanceMap heat = cv::Mat::zeros(h, w, CV_32F);

    for (const auto& face : faces)
    {
        cv::Point center(face.x + face.width / 2, face.y + face.height / 2);
        float radius = std::max(face.width, face.height) * 0.75f;

        for (int y = 0; y < h; y++)
        {
            for (int x = 0; x < w; x++)
            {
                float dx = x - center.x;
                float dy = y - center.y;
                float dist = std::sqrt(dx * dx + dy * dy);

                float value = std::exp(-(dist * dist) / (2 * radius * radius));
                heat.at<float>(y, x) = std::max(heat.at<float>(y, x), value);
            }
        }
    }

    double minVal, maxVal;
    cv::minMaxLoc(heat, &minVal, &maxVal);

    if (maxVal > 1e-6)
        heat /= maxVal;

    return heat;
}
