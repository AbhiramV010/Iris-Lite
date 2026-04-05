#pragma once
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/video.hpp>
#include "Types.hpp"
#include "FaceDetector.hpp"

class ImportanceMapGenerator
{
public:
    // Initialize with frame dimensions and feature selection flags
    ImportanceMapGenerator(int width, int height, bool useFaces, bool useMotion, bool useEdges);

    // Generate importance map (0-1 float) from downscaled frame and optional face heatmap
    ImportanceMap compute(const cv::Mat& smallFrame, const ImportanceMap& faceHeatmap);

private:
    int w, h;
    bool useFaces;
    bool useMotion;
    bool useEdges;

    cv::Ptr<cv::BackgroundSubtractor> bg;   // MOG2 background model
    ImportanceMap prevImportance;           // Previous frame for temporal smoothing
    bool hasPrev = false;

    // Preallocated buffers (avoid per-frame allocation)
    cv::Mat gray_;
    cv::Mat motion_;
    cv::Mat edges_;
    cv::Mat contrast_;
    cv::Mat centerBias_;
    cv::Mat tempFloat_;

    // Internal computation helpers
    ImportanceMap computeMotion(const cv::Mat& gray);      // Foreground detection via MOG2
    ImportanceMap computeEdges(const cv::Mat& gray);       // Edge strength via Scharr operator
    ImportanceMap computeContrast(const cv::Mat& gray);    // Local texture variation
    ImportanceMap computeCenterBias();                     // Photography principle (center importance)
    ImportanceMap normalize(const ImportanceMap& m);       // Rescale to [0, 1] range
};