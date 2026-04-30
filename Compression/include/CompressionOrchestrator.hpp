#pragma once

#include "Config.hpp"
#include "EventTypes.hpp"
#include "ImportanceEngine.hpp"
#include "SystemGovernor.hpp"
#include "CompressionPolicy.hpp"
#include "ImportanceMemory.hpp"

class CompressionOrchestrator
{
public:
    CompressionOrchestrator(
        const Config& cfg,
        ImportanceEngine* importance,
        SystemGovernor* governor,
        CompressionPolicy* policy,
        ImportanceMemory* memory
    );

    struct Decision
    {
        float importance;   // FINAL semantic score
        int crf;
        bool dropFrame;
        int fps; // informational only
    };

    Decision compute(
        const cv::Mat& frame,
        const cv::Mat& prev,
        uint64_t frameIndex,
        uint64_t peakFrame,
        const std::string& trigger
    );

private:
    float fuse(float engineScore, float memoryBias);
    bool shouldDrop(float importance);

private:
    const Config& cfg;

    ImportanceEngine* importance;
    SystemGovernor* governor;
    CompressionPolicy* policy;
    ImportanceMemory* memory;
};