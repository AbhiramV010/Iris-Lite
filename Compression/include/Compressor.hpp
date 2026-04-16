#pragma once

#include <opencv2/core.hpp>
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>

#include "FrameBuffer.hpp"
#include "PerceptualScorer.hpp"
#include "EventAligner.hpp"
#include "H264Encoder.hpp"
#include "Config.hpp"

enum class CompressionMode
{
    IDLE,
    ACTIVE,
    PEAK_PROTECTION
};

class CompressionEngine
{
public:
    CompressionEngine(const Config& cfg);
    ~CompressionEngine();

    bool initialize();

    void pushFrame(const cv::Mat& frame, uint64_t frameIndex);

    void receiveEvent(const DetectionEvent& event);

    void run();

    void shutdown();

    CompressionMode getMode() const { return currentMode; }

    float getAverageCpuLoad() const { return avgCpuLoad; }

private:
    Config cfg;
    std::unique_ptr<FrameBuffer> frameBuffer;
    std::unique_ptr<PerceptualScorer> scorer;
    std::unique_ptr<EventAligner> aligner;
    std::unique_ptr<H264Encoder> encoder;

    CompressionMode currentMode;
    float avgCpuLoad;

    std::queue<cv::Mat> frameQueue;
    std::mutex frameMutex;
    std::condition_variable frameCV;

    std::vector<DetectionEvent> eventQueue;
    std::mutex eventMutex;

    std::thread processingThread;
    std::atomic<bool> running;

    void processingLoop();
    void handleEvent(const DetectionEvent& event);
    void writeClip(const std::vector<BufferedFrame>& frames, const DetectionEvent& event);
    void updateMode();

    cv::Mat preprocessFrame(const cv::Mat& frame);
};