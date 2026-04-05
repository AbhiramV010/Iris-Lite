#include "Quality.hpp"
#include <opencv2/imgproc.hpp>
#include <cmath>
#include "logging.hpp"

double Quality::computeSSIM(const cv::Mat& original, const cv::Mat& compressed)
{
    // Ensure both images are same size
    if (original.size() != compressed.size())
    {
        logError("Cannot compute SSIM: image sizes differ");
        return -1.0;
    }

    // Convert to grayscale if multi-channel
    cv::Mat orig_gray, comp_gray;
    if (original.channels() == 3)
    {
        cv::cvtColor(original, orig_gray, cv::COLOR_BGR2GRAY);
        cv::cvtColor(compressed, comp_gray, cv::COLOR_BGR2GRAY);
    }
    else
    {
        orig_gray = original.clone();
        comp_gray = compressed.clone();
    }

    // Convert to float for computation
    cv::Mat orig_f, comp_f;
    orig_gray.convertTo(orig_f, CV_32F);
    comp_gray.convertTo(comp_f, CV_32F);

    // SSIM constants (from literature)
    const double C1 = 6.5025;    // (0.01 * 255)^2
    const double C2 = 58.5225;   // (0.03 * 255)^2

    // Compute local means via Gaussian blur
    cv::Mat mean_orig, mean_comp;
    cv::GaussianBlur(orig_f, mean_orig, cv::Size(11, 11), 1.5);
    cv::GaussianBlur(comp_f, mean_comp, cv::Size(11, 11), 1.5);

    // Compute local variances and covariance
    cv::Mat sigma_orig_sq, sigma_comp_sq, sigma_covar;
    cv::GaussianBlur(orig_f.mul(orig_f), sigma_orig_sq, cv::Size(11, 11), 1.5);
    sigma_orig_sq = sigma_orig_sq - mean_orig.mul(mean_orig);

    cv::GaussianBlur(comp_f.mul(comp_f), sigma_comp_sq, cv::Size(11, 11), 1.5);
    sigma_comp_sq = sigma_comp_sq - mean_comp.mul(mean_comp);

    cv::GaussianBlur(orig_f.mul(comp_f), sigma_covar, cv::Size(11, 11), 1.5);
    sigma_covar = sigma_covar - mean_orig.mul(mean_comp);

    // SSIM formula: (2*mu1*mu2 + C1)(2*sigma12 + C2) / ((mu1^2 + mu2^2 + C1)(sigma1^2 + sigma2^2 + C2))
    cv::Mat numerator = (2 * mean_orig.mul(mean_comp) + C1).mul(2 * sigma_covar + C2);
    cv::Mat denominator = (mean_orig.mul(mean_orig) + mean_comp.mul(mean_comp) + C1).mul(
        sigma_orig_sq + sigma_comp_sq + C2);

    cv::Mat ssim_map;
    cv::divide(numerator, denominator, ssim_map);

    // Return mean SSIM across entire image
    double ssim_value = cv::mean(ssim_map)[0];
    return ssim_value;
}

double Quality::computePSNR(const cv::Mat& original, const cv::Mat& compressed)
{
    // Ensure same size
    if (original.size() != compressed.size())
    {
        logError("Cannot compute PSNR: image sizes differ");
        return -1.0;
    }

    // Compute absolute difference between frames
    cv::Mat diff;
    cv::absdiff(original, compressed, diff);
    diff.convertTo(diff, CV_32F);

    // Square the differences for MSE
    diff = diff.mul(diff);

    // Mean Squared Error
    double mse = cv::mean(diff)[0];

    // Handle perfect match (MSE = 0)
    if (mse < 1e-10)
        return 100.0;  // Perfect reconstruction

    // PSNR formula: 20 * log10(255 / sqrt(MSE))
    double psnr = 20.0 * std::log10(255.0 / std::sqrt(mse));
    return psnr;
}