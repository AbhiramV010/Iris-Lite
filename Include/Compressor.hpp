#pragma once
#include <string>
#include <opencv2/opencv.hpp>

// This class handles reading a video, running the ROI processor,
// scoring motion, and saving the compressed output.

class Compressor {
public:
    bool compressVideo(const std::string& inputPath, const std::string& outputPath);

private:
    // Store previous frame for motion scoring
    cv::Mat prevFrame_;
};
