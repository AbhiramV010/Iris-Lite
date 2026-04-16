#include "Compressor.hpp"
#include "logging.hpp"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>

CompressionEngine::CompressionEngine(const Config& cfg_)
    : cfg(cfg_), currentMode(CompressionMode::IDLE), avgCpuLoad(0.0f), running(false)
{
    frameBuffer = std::make_unique<FrameBuffer>(300, cfg.encodeWidth, cfg.encodeHeight);
    scorer = std::make_unique<PerceptualScorer>(cfg.perceptualWidth, cfg.perceptualHeight);
    aligner = std::make_unique<EventAligner>();
    encoder = std::make_unique<H264Encoder>(cfg.encodeWidth, cfg.encodeHeight, 30, 1200, true);
}

CompressionEngine::~CompressionEngine()
{
    shutdown();
}

bool CompressionEngine::initialize()
{
    logInfo("CompressionEngine initializing");

    if (!frameBuffer || !scorer || !aligner || !encoder)
    {
        logError("Component initialization failed");
        return false;
    }

    running = true;
    processingThread = std::thread(&CompressionEngine::processingLoop, this);

    logInfo("CompressionEngine initialized and running");
    return true;
}

void CompressionEngine::pushFrame(const cv::Mat& frame, uint64_t frameIndex)
{
    cv::Mat preprocessed = preprocessFrame(frame);

    frameBuffer->pushFrame(preprocessed, frameIndex);

    float score = scorer->scoreFrame(preprocessed, frameBuffer->getLatestFrame().frame);

    {
        std::lock_guard<std::mutex> lock(frameMutex);
        frameQueue.push(preprocessed);
        if (frameQueue.size() > 10)
            frameQueue.pop();
    }

    frameCV.notify_one();
    updateMode();
}

void CompressionEngine::receiveEvent(const DetectionEvent& event)
{
    std::lock_guard<std::mutex> lock(eventMutex);
    eventQueue.push_back(event);
    logInfo("Event queued: " + event.triggerType);
}

void CompressionEngine::run()
{
    while (running)
    {
        std::unique_lock<std::mutex> lock(eventMutex);

        if (!eventQueue.empty())
        {
            DetectionEvent event = eventQueue.front();
            eventQueue.erase(eventQueue.begin());
            lock.unlock();

            handleEvent(event);
        }
        else
        {
            lock.unlock();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

void CompressionEngine::shutdown()
{
    running = false;
    frameCV.notify_all();

    if (processingThread.joinable())
        processingThread.join();

    if (encoder && encoder->isOpen())
        encoder->close();

    logInfo("CompressionEngine shutdown complete");
}

void CompressionEngine::processingLoop()
{
    logInfo("Processing loop started");

    while (running)
    {
        std::unique_lock<std::mutex> lock(frameMutex);
        frameCV.wait_for(lock, std::chrono::milliseconds(500));

        if (!frameQueue.empty())
        {
            cv::Mat frame = frameQueue.front();
            frameQueue.pop();
            lock.unlock();
        }
    }

    logInfo("Processing loop exited");
}

void CompressionEngine::handleEvent(const DetectionEvent& event)
{
    uint64_t startIdx = aligner->mapTimeToFrameIndex(event.startTimeStr);
    uint64_t endIdx = aligner->mapTimeToFrameIndex(event.endTimeStr);

    auto frames = frameBuffer->getFrameRange(startIdx, endIdx);

    if (frames.empty())
    {
        logWarn("No frames found for event: " + event.triggerType);
        return;
    }

    writeClip(frames, event);
}

void CompressionEngine::writeClip(const std::vector<BufferedFrame>& frames, const DetectionEvent& event)
{
    if (frames.empty())
        return;

    std::string outputFile = "output_" + event.triggerType + ".mp4";

    if (!encoder->open(outputFile))
    {
        logError("Failed to open encoder for clip");
        return;
    }

    for (const auto& bf : frames)
    {
        if (!encoder->writeFrame(bf.frame))
        {
            logError("Failed to write frame to encoder");
            break;
        }
    }

    encoder->close();

    logInfo("Clip written: " + outputFile + " (" + std::to_string(frames.size()) + " frames)");

    try
    {
        std::string metaFile = outputFile + ".json";
        std::ofstream meta(metaFile);
        meta << "{\n";
        meta << "  \"trigger\": \"" << event.triggerType << "\",\n";
        meta << "  \"startTime\": \"" << event.startTimeStr << "\",\n";
        meta << "  \"endTime\": \"" << event.endTimeStr << "\",\n";
        meta << "  \"duration\": " << event.duration << ",\n";
        meta << "  \"frames\": " << frames.size() << ",\n";
        meta << "  \"isMotionSensor\": " << (event.isMotionSensor ? "true" : "false") << ",\n";
        meta << "  \"isDoorSensor\": " << (event.isDoorSensor ? "true" : "false") << "\n";
        meta << "}\n";
        meta.close();

        logInfo("Metadata written: " + metaFile);
    }
    catch (const std::exception& e)
    {
        logError("Failed to write metadata: " + std::string(e.what()));
    }
}

void CompressionEngine::updateMode()
{
    float motionScore = scorer->getMotionScore();

    if (motionScore > 0.7f)
        currentMode = CompressionMode::PEAK_PROTECTION;
    else if (motionScore > 0.4f)
        currentMode = CompressionMode::ACTIVE;
    else
        currentMode = CompressionMode::IDLE;
}

cv::Mat CompressionEngine::preprocessFrame(const cv::Mat& frame)
{
    if (frame.empty())
        return frame;

    cv::Mat processed = frame.clone();

    for (const auto& zone : cfg.privacyZones)
    {
        cv::rectangle(processed, zone, cv::Scalar(0, 0, 0), cv::FILLED);
    }

    return processed;
}