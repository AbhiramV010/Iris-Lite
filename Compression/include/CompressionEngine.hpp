#pragma once

#include <memory>
#include <atomic>
#include <string>
#include <vector>

#include "ImportanceEngine.hpp"
#include "CompressionPolicy.hpp"
#include "H264Encoder.hpp"
#include "SharedFrameBuffer.hpp"

struct EventWindow
{
    uint64_t startFrame;
    uint64_t endFrame;
    std::string trigger;
};

class CompressionEngine
{
public:
    bool initialize(const Config& cfg);
    void shutdown();

    void processEvent(const EventWindow& event);

private:
    std::atomic<bool> running{ false };

    std::unique_ptr<ImportanceEngine> importance;
    std::unique_ptr<CompressionPolicy> policy;
    std::unique_ptr<H264Encoder> encoder;
    std::unique_ptr<SharedFrameBuffer> sharedBuffer;

    float importanceState = 0.5f;
    float momentum = 0.5f;
    bool initialize(const Config& cfg);
    int currentCRF = -1;
    int segmentIndex = 0;

    void startNewSegment(int crf);
};