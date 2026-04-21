#include "CompressionEngine.hpp"
#include "SnapshotExtractor.hpp"

#include <opencv2/opencv.hpp>
#include <algorithm>
#include <cmath>

// ---------------- INITIALIZE ----------------

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

// ---------------- SEGMENT CONTROL ----------------

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

// ---------------- EVENT PROCESSING ----------------

void CompressionEngine::processEvent(const EventWindow& event)
{
    if (!running)
        return;

    // ---------- SNAPSHOT ----------
    auto snapshot = SnapshotExtractor::extract(
        *sharedBuffer,
        event.startFrame,
        event.endFrame
    );

    if (snapshot.empty())
        return;

    cv::Mat prev;

    for (auto& jpeg : snapshot)
    {
        // ---------- DECODE ----------
        cv::Mat frame = cv::imdecode(jpeg, cv::IMREAD_COLOR);
        if (frame.empty())
            continue;

        // ---------- IMPORTANCE ----------
        auto signal = importance->analyze(frame, prev);

        float imp =
            0.5f * signal.motion +
            0.3f * signal.edges +
            0.2f * signal.faces;

        // ---------- TEMPORAL SMOOTHING ----------
        importanceState = 0.9f * importanceState + 0.1f * imp;
        momentum = 0.85f * momentum + 0.15f * importanceState;

        // ---------- CRF DECISION ----------
        int crf = policy->computeCRF(importanceState, momentum, 28);

        // reduce CRF thrashing (important for Pi)
        if (currentCRF == -1 || std::abs(crf - currentCRF) >= 3)
        {
            startNewSegment(crf);
        }

        // ---------- ENCODE ----------
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

    // ---------- CLEANUP ----------
    if (encoder->isOpen())
        encoder->close();
}

// ---------------- SHUTDOWN ----------------

void CompressionEngine::shutdown()
{
    running = false;

    if (encoder)
        encoder->close();
}