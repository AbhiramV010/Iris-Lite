#include "CompressionEngine.hpp"
#include "SnapshotExtractor.hpp"

#include <opencv2/opencv.hpp>
#include <cmath>
#include <cstdint>
#include <string>
#include <format>
#include <chrono>
#include <iostream>

bool CompressionEngine::initialize(const Config& cfg)
{
    importance = std::make_unique<ImportanceEngine>(
        cfg.perceptualWidth,
        cfg.perceptualHeight,
        cfg
    );

    policy = std::make_unique<CompressionPolicy>();

    encoder = std::make_unique<H264Encoder>(
        cfg.encodeWidth,
        cfg.encodeHeight,
        cfg.fps,
        true
    );

    sharedBuffer = std::make_unique<SharedFrameBuffer>();
    if (!sharedBuffer->initialize())
        return false;

    importanceState = 0.0f;
    momentum = 0.0f;
    currentCRF = -1;
    running = true;
    return true;
}

void CompressionEngine::startNewSegment(const std::string& fileName, int crf)
{
    if (encoder->isOpen())
        encoder->close();

    if (!encoder->open(fileName, crf))
    {
        running = false;
        return;
    }

    currentCRF = crf;   
}

void CompressionEngine::processEvent(const EventWindow& event)
{
    if (!running)
        return;

    auto snapshot = SnapshotExtractor::extract(
        *sharedBuffer,
        event.startFrame,
        event.endFrame
    );

    if (snapshot.empty())
        return;

    cv::Mat prev;
    uint64_t frameIndex = 0;
    int processed = 0;
    const int maxFrames = 300; // safety cap

    for (auto& jpeg : snapshot)
    {
        if (processed++ > maxFrames)
            break;

        // skip tiny/invalid frames (cheap optimization)
        if (jpeg.size() < 2000)
            continue;

        cv::Mat frame = cv::imdecode(jpeg, cv::IMREAD_COLOR);
        if (frame.empty())
            continue;

        auto signal = importance->analyze(frame, prev);
        float imp = signal.global;

        // smoothing
        importanceState = 0.9f * importanceState + 0.1f * imp;
        momentum = 0.85f * momentum + 0.15f * importanceState;

        //  FRAME DROPPING (REAL)
        bool keep = policy->shouldKeepFrame(
            importanceState,
            momentum,
            frameIndex++
        );

        if (!keep)
            continue;

        //  SMART CRF BASE
        int base = 26;
        if (importanceState > 0.7f)
            base = 20;
        else if (importanceState < 0.3f)
            base = 32;

        int crf = policy->computeCRF(importanceState, momentum, base);

        //  reduce thrashing
        if (!encoder->isOpen() || (currentCRF != -1 && std::abs(crf - currentCRF) >= 5))
        {
            std::string name = "segment_" + std::to_string(segmentIndex++) + ".mp4";
            startNewSegment(name, crf);
        }

        //  encode
        if (encoder->isOpen())
        {
            if (!encoder->writeFrame(frame))
            {
                running = false;
                break;
            }
        }
        prev = frame;
    }
}

void CompressionEngine::takeClip(uint64_t START_IDX, uint64_t END_IDX) {
    auto now = std::chrono::system_clock::now();
    std::string ts = std::format("{:%Y-%m-%d_%H-%M-%S}", now);
    std::string fullPath = std::format("/clipDrive/clips/iris_lite--{}.mp4", ts);

    EventWindow event;
    event.startFrame = START_IDX;
    event.endFrame = END_IDX;
    event.trigger = "manual_take_clip";

    startNewSegment(fullPath, 26);
    processEvent(event); 
    
    if (encoder->isOpen())
        encoder->close();
}

void CompressionEngine::shutdown()
{
    running = false;
    if (encoder)
        encoder->close();
}