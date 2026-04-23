#include "CompressionEngine.hpp"
#include "SnapshotExtractor.hpp"

#include <opencv2/opencv.hpp>
#include <cmath>
#include <cstdint>
#include <string>
#include <format>
#include <chrono>
#include <iostream>

auto now = std::chrono::system_clock::now();

const std::string TIME_STAMP = std::format("{:%Y-%m-%d %H:%M:%S}", now);
const std::string FILE_PATH = std::format("/clipDrive/clips/{}.mp4"); 
const std::string BUFFER_SHM = std::format("/iris_frame_buffer_data"); // frame buffer
const std::string DATA_SHM = std::format("/iris_indices"); // metadata

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

    running = true;
    return true;
}

void CompressionEngine::startNewSegment(int crf)
{
    if (encoder->isOpen())
        encoder->close();

    std::string name = "event_" + std::to_string(segmentIndex++) + ".mp4";

    if (!encoder->open(name, crf))
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
        if (currentCRF == -1 || std::abs(crf - currentCRF) >= 5)
            startNewSegment(crf);

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

    if (encoder->isOpen())
        encoder->close();
}

bool CompressionEngine::takeClip(uint64_t START_IDX, uint64_t END_IDX) { // function written to take a clip,  
    if (END_IDX < START_IDX) {
        std::cout << "END_IDX is LESS than START_IDX " << std::endl;
        return false;
    } else { // logic for taking the clip
        int fd;
        
        // TODO
        for (int c=START_IDX; c <= END_IDX) { // LEQ because we need to count the last frame (it don't make a diff, but it needs to work properly, no?)
            // this loops starting from the beginning frame indice to the very end frame indice of the 'event of concern' provided by py
            // what I'm confused about is how to take the clip that was COMPRESSED, and not from the raw memory buffer
        }
    }
}

void CompressionEngine::shutdown()
{
    running = false;

    if (encoder)
        encoder->close();
}