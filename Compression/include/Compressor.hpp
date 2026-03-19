#pragma once
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <opencv2/core.hpp>
#include "Config.hpp"

#include "Types.hpp"
#include "ImportanceMap.hpp"
#include "FaceDetector.hpp"
#include "FrameProcessor.hpp"
#include "PrivacyMask.hpp"

class Compressor
{
public:
    Compressor(const Config& cfg);
    ~Compressor();

    // Start processing + encoding threads
    bool start(const std::string& outputPath);

    // Push a new raw frame (full resolution)
    void pushFrame(const FrameInfo& frame);

    // Stop threads and close FFmpeg
    void stop();

private:
    // Threads
    std::thread processingThread;
    std::thread encodingThread;

    std::atomic<bool> running{ false };

    // Frame queue (processed frames ready for FFmpeg)
    std::queue<cv::Mat> frameQueue;
    std::mutex queueMutex;
    std::condition_variable queueCV;

    // FFmpeg pipe
    FILE* ffmpegPipe = nullptr;

    // Modules
    ImportanceMapGenerator* importanceGen = nullptr;
    FaceDetector* faceDetector = nullptr;
    FrameProcessor* frameProcessor = nullptr;
    PrivacyMask* privacyMask = nullptr;

    Config cfg;

    // Internal thread loops
    void processingLoop();
    void encodingLoop();

    // Queue helpers
    void enqueueFrame(const cv::Mat& frame);
    cv::Mat dequeueFrame();
};
