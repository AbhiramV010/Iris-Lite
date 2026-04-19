#pragma once

#include <opencv2/opencv.hpp>
#include <memory>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <unordered_set>
#include <atomic>
#include <string>

#include "Config.hpp"
#include "ImportanceEngine.hpp"
#include "ImportanceMap.hpp"
#include "ImportanceResult.hpp"
#include "CompressionPolicy.hpp"
#include "AdaptiveEncoderController.hpp"
#include "H264encoder.hpp"
#include "TriggerEngine.hpp"
class CompressionEngine
{
    
public:
    explicit CompressionEngine(const Config& cfg);
    ~CompressionEngine();

    bool initialize();
    void shutdown();

    void pushFrame(const cv::Mat& frame, uint64_t idx);

private:
    // 🔧 CORE PIPELINE
    void processVideoFile(const std::string& path);
    struct FastPathState
    {
        cv::Mat prevFrame;
        float motion = 0.0f;
        uint64_t frameIdx = 0;
    };

private:
    Config cfg;
    std::atomic<bool> running{ false };

    std::unique_ptr<ImportanceEngine> importanceEngine;
    std::unique_ptr<ImportanceMap> importanceMap;
    std::unique_ptr<CompressionPolicy> policy;
    std::unique_ptr<AdaptiveEncoderController> controller;
    std::unique_ptr<H264Encoder> encoder;
    std::unique_ptr<TriggerEngine> triggerEngine;

};