#include "FrameProcessor.hpp"
#include <opencv2/imgproc.hpp>
#include <opencv2/core.hpp>
#include "logging.hpp"

FrameProcessor::FrameProcessor(int width, int height)
    : w(width), h(height)
{
    logInfo("FrameProcessor SAFE MODE (FINAL FIX)");
}

static cv::Mat toFloat3(const cv::Mat& img)
{
    cv::Mat f;
    img.convertTo(f, CV_32F, 1.0 / 255.0);

    if (f.channels() == 1)
        cv::cvtColor(f, f, cv::COLOR_GRAY2BGR);

    return f;
}

cv::Mat FrameProcessor::createSmoothBilateral(const cv::Mat& frame)
{
    cv::Mat blurred;
    cv::bilateralFilter(frame, blurred, 9, 75, 75);
    return blurred;
}

cv::Mat FrameProcessor::process(const cv::Mat& fullFrame, const ImportanceMap& importance)
{
    if (fullFrame.empty() || importance.empty()) {
        logInfo("CRASH SAFE EXIT: empty mat");
        return fullFrame;
    }

    logInfo("FRAME: " +
        std::to_string(fullFrame.cols) + "x" +
        std::to_string(fullFrame.rows));

    logInfo("IMP: " +
        std::to_string(importance.cols) + "x" +
        std::to_string(importance.rows));

    cv::Size size = fullFrame.size();

    // ------------------------
    // FRAME PREP (FORCE CONSISTENCY)
    // ------------------------
    cv::Mat sharp = fullFrame.clone();
    cv::Mat smooth = createSmoothBilateral(fullFrame);

    if (smooth.size() != size)
        cv::resize(smooth, smooth, size);

    cv::Mat sharpF = toFloat3(sharp);
    cv::Mat smoothF = toFloat3(smooth);

    // ------------------------
    // IMPORTANCE SAFE CONVERSION
    // ------------------------
    cv::Mat imp;

    if (importance.channels() == 3)
        cv::cvtColor(importance, imp, cv::COLOR_BGR2GRAY);
    else
        imp = importance.clone();

    if (imp.size() != size)
        cv::resize(imp, imp, size);

    imp.convertTo(imp, CV_32F, 1.0 / 255.0);

    cv::threshold(imp, imp, 1.0, 1.0, cv::THRESH_TRUNC);
    cv::threshold(imp, imp, 0.0, 0.0, cv::THRESH_TOZERO);

    cv::Mat imp3;
    cv::Mat c[] = { imp, imp, imp };
    cv::merge(c, 3, imp3);

    // ------------------------
    // FINAL SAFETY ASSERTIONS
    // ------------------------
    CV_Assert(sharpF.size() == smoothF.size());
    CV_Assert(sharpF.size() == imp3.size());
    CV_Assert(sharpF.type() == CV_32FC3);
    CV_Assert(smoothF.type() == CV_32FC3);
    CV_Assert(imp3.type() == CV_32FC3);

    // ------------------------
    // BLENDING (NO ALIASING)
    // ------------------------
    cv::Mat oneMinus;
    cv::subtract(cv::Scalar(1, 1, 1), imp3, oneMinus);

    cv::Mat a, b, out;
    cv::multiply(imp3, sharpF, a);
    cv::multiply(oneMinus, smoothF, b);
    cv::add(a, b, out);

    cv::Mat result;
    out.convertTo(result, CV_8U, 255.0);

    return result;
}