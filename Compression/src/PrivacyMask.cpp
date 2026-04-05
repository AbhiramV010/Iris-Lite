#include "PrivacyMask.hpp"
#include "Utils.hpp"
#include <opencv2/imgproc.hpp>
#include "logging.hpp"

PrivacyMask::PrivacyMask(const std::vector<cv::Rect>& zones_)
    : zones(zones_)
{
    logInfo("Privacy masking initialized with " + std::to_string(zones.size()) + " protected regions");
}

void PrivacyMask::apply(cv::Mat& frame)
{
    // Apply opaque rectangles to user-defined sensitive regions.
    for (const auto& r : zones)
    {
        cv::rectangle(frame, r, cv::Scalar(0, 0, 0), cv::FILLED);
    }
}