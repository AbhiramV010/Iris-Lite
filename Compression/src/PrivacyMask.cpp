#include "PrivacyMask.hpp"
#include "Utils.hpp"
#include <opencv2/imgproc.hpp>
#include "logging.hpp"

PrivacyMask::PrivacyMask(const std::vector<cv::Rect>& zones_)
    : zones(zones_)
{
    logInfo("Initialized PrivacyMask with " + std::to_string(zones.size()) + " zones");
}

void PrivacyMask::apply(cv::Mat& frame)
{
    for (const auto& r : zones)
    {
        cv::rectangle(frame, r, cv::Scalar(0, 0, 0), cv::FILLED);
    }
}
