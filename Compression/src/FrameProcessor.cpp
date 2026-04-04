#include "FrameProcessor.hpp"
#include "Utils.hpp"
#include <opencv2/imgproc.hpp>
#include "Config.hpp"
#include "logging.hpp"

FrameProcessor::FrameProcessor(int width, int height)
    : w(width), h(height)
{
    logInfo("FrameProcessor initialized");

    // Preallocate buffers
    smooth_.create(h, w, CV_8UC3);
    impResized_.create(h, w, CV_32F);
    imp3_.create(h, w, CV_32FC3);
    sharpF_.create(h, w, CV_32FC3);
    smoothF_.create(h, w, CV_32FC3);
    outF_.create(h, w, CV_32FC3);
    oneMinusImp_.create(h, w, CV_32FC3);
}

cv::Mat FrameProcessor::createSmooth(const cv::Mat& frame)
{
    // Fast smoothing
    cv::GaussianBlur(frame, smooth_, cv::Size(9, 9), 0);
    return smooth_;
}

cv::Mat FrameProcessor::process(const cv::Mat& fullFrame, const ImportanceMap& importance)
{
    // 1. Sharp + smooth
    const cv::Mat& sharp = fullFrame;
    const cv::Mat& smooth = createSmooth(fullFrame);

    // 2. Resize importance map
    cv::resize(importance, impResized_, fullFrame.size(), 0, 0, cv::INTER_LINEAR);

    // 3. Expand to 3 channels (safe + compatible)
    cv::Mat channels[] = { impResized_, impResized_, impResized_ };
    cv::merge(channels, 3, imp3_);

    // 4. Convert sharp + smooth to float
    sharp.convertTo(sharpF_, CV_32F);
    smooth.convertTo(smoothF_, CV_32F);

    // 5. Compute (1 - imp)
    cv::subtract(1.0f, imp3_, oneMinusImp_);

    // 6. out = imp * sharp + (1 - imp) * smooth
    cv::multiply(imp3_, sharpF_, outF_);
    cv::Mat tmp;
    cv::multiply(oneMinusImp_, smoothF_, tmp);
    outF_ += tmp;

    // 7. Convert back to 8-bit
    cv::Mat out;
    outF_.convertTo(out, CV_8U);
    return out;
}
