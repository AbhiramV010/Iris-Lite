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
#include "Quality.hpp"

class Compressor
{
public:
    Compressor(const Config& cfg);
    ~Compressor();

    // Initialize FFmpeg pipe and start processing threads
    bool start(const std::string& outputPath);

    // Queue frame for processing and encoding
    void pushFrame(const FrameInfo& frame);

    // Stop all threads and close FFmpeg pipe
    void stop();

private:
    std::thread processingThread;
    std::thread encodingThread;
    std::atomic<bool> running{ false };

    std::queue<cv::Mat> frameQueue;
    std::mutex queueMutex;
    std::condition_variable queueCV;

    FILE* ffmpegPipe = nullptr;

    ImportanceMapGenerator* importanceGen = nullptr;
    FaceDetector* faceDetector = nullptr;
    FrameProcessor* frameProcessor = nullptr;
    PrivacyMask* privacyMask = nullptr;

    Config cfg;

    void processingLoop();
    void encodingLoop();
    void enqueueFrame(const cv::Mat& frame);
    cv::Mat dequeueFrame();
};