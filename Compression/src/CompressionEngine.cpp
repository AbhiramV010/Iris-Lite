#include "CompressionEngine.hpp"
#include "SnapshotExtractor.hpp"

#include <opencv2/opencv.hpp>
#include <cmath>

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

void CompressionEngine::shutdown()
{
    running = false;

    if (encoder)
        encoder->close();
}