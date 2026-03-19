#include "FrameProcessor.hpp"
#include "Utils.hpp"
#include <opencv2/imgproc.hpp>
#include "Config.hpp"
#include "logging.hpp"

FrameProcessor::FrameProcessor(int width, int height)
    : w(width), h(height)
{
    logInfo("FrameProcessor initialized");
}

cv::Mat FrameProcessor::createSmooth(const cv::Mat& frame)
{
    cv::Mat smooth;

    int diameter = 9;
    double sigmaColor = 50;
    double sigmaSpace = 50;

    cv::bilateralFilter(frame, smooth, diameter, sigmaColor, sigmaSpace);

    return smooth;
}

cv::Mat FrameProcessor::process(const cv::Mat& fullFrame, const ImportanceMap& importance)
{
    cv::Mat sharp = fullFrame;
    cv::Mat smooth = createSmooth(fullFrame);

    cv::Mat impResized;
    cv::resize(importance, impResized, fullFrame.size(), 0, 0, cv::INTER_LINEAR);

    cv::Mat imp3;
    cv::Mat channels[] = { impResized, impResized, impResized };
    cv::merge(channels, 3, imp3);

    cv::Mat sharpF, smoothF, outF;
    sharp.convertTo(sharpF, CV_32F);
    smooth.convertTo(smoothF, CV_32F);

    outF = imp3.mul(sharpF) + (1.0f - imp3).mul(smoothF);

    cv::Mat out;
    outF.convertTo(out, CV_8U);

    return out;
}
