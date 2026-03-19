#pragma once
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/video.hpp>   // <-- MOG2 lives here
#include "Types.hpp"
#include "FaceDetector.hpp"

class ImportanceMapGenerator
{
public:
    ImportanceMapGenerator(int width, int height, bool useFaces, bool useMotion, bool useEdges);

    // Process a downscaled frame and return importance map (float 0–1)
    ImportanceMap compute(const cv::Mat& smallFrame, const ImportanceMap& faceHeatmap);

private:
    int w, h;
    bool useFaces;
    bool useMotion;
    bool useEdges;

    cv::Ptr<cv::BackgroundSubtractor> bg;   // MOG2 (correct)
    ImportanceMap prevImportance;           // for temporal smoothing
    bool hasPrev = false;

    // Internal helpers
    ImportanceMap computeMotion(const cv::Mat& gray);
    ImportanceMap computeEdges(const cv::Mat& gray);
    ImportanceMap computeContrast(const cv::Mat& gray);
    ImportanceMap computeCenterBias();
    ImportanceMap normalize(const ImportanceMap& m);
};
