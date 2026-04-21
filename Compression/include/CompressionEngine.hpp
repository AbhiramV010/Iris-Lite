#pragma once

#include <opencv2/opencv.hpp>
#include <memory>
#include <atomic>
#include <string>
#include <vector>
#include <deque>
#include <cstdint>

#include "FrameWindowBuffer.hpp"
#include "Config.hpp"
#include "ImportanceEngine.hpp"
#include "CompressionPolicy.hpp"
#include "H264encoder.hpp"
#include "EventAligner.hpp"

// ---------------- EVENT INPUT FROM PYTHON ----------------
struct EventWindow
{
    uint64_t startFrame;
    uint64_t endFrame;
    std::string trigger;
};

// ---------------- FRAME PACKAGE ----------------
struct FramePacket
{
    cv::Mat frame;
    uint64_t frameIndex;

    float importance;
    float motion;
    float edges;
    float faces;
};

class CompressionEngine
{
public:
    explicit CompressionEngine(const Config& cfg);
    ~CompressionEngine();

    bool initialize();
    void shutdown();

    void processVideoFile(const std::string& path);
    void processEvent(const EventWindow& event);

private:
    Config cfg;
    std::atomic<bool> running{ false };

    // ---------------- CORE MODULES ----------------
    std::unique_ptr<ImportanceEngine> importanceEngine;
    std::unique_ptr<CompressionPolicy> policy;
    std::unique_ptr<H264Encoder> encoder;
    std::unique_ptr<EventAligner> aligner;

    // ---------------- TEMPORAL STATE ----------------
    float importanceState = 0.5f;
    float momentum = 0.5f;

    // ---------------- SEGMENT CONTROL ----------------
    int currentCRF = -1;
    int segmentIndex = 0;

    // ---------------- BUFFER (PI OPTIMIZED) ----------------
    std::unique_ptr<FrameWindowBuffer> buffer;

    // ---------------- INTERNAL PIPELINE ----------------
    void startNewSegment(int crf);
    void clearBuffer();
    void pushFrame(const FramePacket& packet);
    void processWindow(uint64_t startFrame, uint64_t endFrame);
};