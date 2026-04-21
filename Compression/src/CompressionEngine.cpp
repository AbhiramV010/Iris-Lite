#include "CompressionEngine.hpp"
#include "logging.hpp"
#include "ImportanceSignal.hpp"

#include <opencv2/opencv.hpp>
#include <algorithm>

// ---------------- CONSTRUCTOR ----------------

CompressionEngine::CompressionEngine(const Config& cfg_)
    : cfg(cfg_)
{
}

CompressionEngine::~CompressionEngine()
{
    shutdown();
}

// ---------------- INIT ----------------

bool CompressionEngine::initialize()
{
    logInfo("IRIS-Lite Event-Driven Compression Engine initializing");

    importanceEngine = std::make_unique<ImportanceEngine>(
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

    aligner = std::make_unique<EventAligner>();

    buffer = std::make_unique<FrameWindowBuffer>(cfg.bufferSize);

    running = true;
    return true;
}

// ---------------- BUFFER MANAGEMENT ----------------

void CompressionEngine::clearBuffer()
{
    buffer->clear();
}

void CompressionEngine::pushFrame(const FramePacket& packet)
{
    std::vector<uint8_t> dummyJpeg; // placeholder bridge

    buffer->pushFrame(packet.frameIndex, dummyJpeg);
}

// ---------------- SEGMENT CONTROL ----------------

void CompressionEngine::startNewSegment(int crf)
{
    if (encoder && encoder->isOpen())
        encoder->close();

    std::string filename =
        "output_" + std::to_string(segmentIndex++) + ".mp4";

    if (!encoder->open(filename, crf))
    {
        logError("Failed to open encoder segment");
        running = false;
    }

    currentCRF = crf;
}

// ---------------- WINDOW PROCESSING ----------------

void CompressionEngine::processWindow(uint64_t startFrame, uint64_t endFrame)
{
    if (!buffer)
        return;

    if (startFrame > endFrame)
        return;

    logInfo("Processing event window");

    auto frames = buffer->getFrameRange(startFrame, endFrame);

    for (size_t i = 0; i < frames.size(); i++)
    {
        const cv::Mat& frame = frames[i];

        ImportanceSignal signal = importanceEngine->analyze(
            frame,
            (i > 0) ? frames[i - 1] : frame
        );

        float importance =
            0.5f * signal.motion +
            0.3f * signal.edges +
            0.2f * signal.faces;

        importanceState = 0.9f * importanceState + 0.1f * importance;
        importanceState = std::clamp(importanceState, 0.0f, 1.0f);

        momentum = 0.85f * momentum + 0.15f * importanceState;
        momentum = std::clamp(momentum, 0.0f, 1.0f);

        int crf = policy->computeCRF(
            importanceState,
            momentum,
            cfg.crf
        );

        if (currentCRF == -1 || std::abs(crf - currentCRF) >= 3)
        {
            startNewSegment(crf);
        }

        if (encoder && encoder->isOpen())
        {
            if (!encoder->writeFrame(frame))
            {
                logError("Encoding failure in event window");
                running = false;
                return;
            }
        }
    }
}

// ---------------- MAIN PIPELINE ----------------

void CompressionEngine::processVideoFile(const std::string& path)
{
    cv::VideoCapture cap(path);

    if (!cap.isOpened())
    {
        logError("Failed to open video: " + path);
        return;
    }

    cv::Mat frame;
    uint64_t idx = 0;

    clearBuffer();

    while (cap.read(frame) && running)
    {
        if (frame.empty())
        {
            idx++;
            continue;
        }

        FramePacket pkt;
        pkt.frame = frame.clone();
        pkt.frameIndex = idx;

        pushFrame(pkt);

        bool eventTriggered = policy->shouldKeepFrame(
            importanceState,
            momentum,
            idx
        );

        if (!eventTriggered)
        {
            idx++;
            continue;
        }

        auto window = aligner->align(
            idx,
            idx,
            cfg.bufferSize,
            cfg.estimatedLatency
        );

        processWindow(window.startFrame, window.endFrame);

        idx++;
    }

    if (encoder)
        encoder->close();

    logInfo("Processing complete");
    logInfo("Frames processed: " + std::to_string(idx));
}

// ---------------- SHUTDOWN ----------------

void CompressionEngine::shutdown()
{
    if (!running)
        return;

    running = false;

    if (encoder)
        encoder->close();

    logInfo("CompressionEngine shutdown complete");
}