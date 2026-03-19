#include "Compressor.hpp"
#include "Utils.hpp"
#include <opencv2/imgproc.hpp>
#include <cstdio>
#include <sstream>
#include "Config.hpp"
#include "logging.hpp"


// Build FFmpeg command 

static std::string buildFFmpegCommand(
    const std::string& outputPath,
    int width,
    int height,
    int crf,
    const std::string& mode)
{
    std::ostringstream cmd;

    cmd << "ffmpeg "
        << "-f rawvideo "
        << "-pix_fmt bgr24 "
        << "-s " << width << "x" << height << " "
        << "-r 15 "
        << "-i - "
        << "-c:v libx264 "
        << "-preset slow "
        << "-crf " << crf << " "
        << "-pix_fmt yuv420p "
        << "-movflags +faststart "
        << "-y \"" << outputPath << "\"";

    return cmd.str();
}

// Constructor / Destructor
Compressor::Compressor(const Config& cfg_)
    : cfg(cfg_)
{
    importanceGen = new ImportanceMapGenerator(
        cfg.perceptualWidth,
        cfg.perceptualHeight,
        cfg.useFaces,
        cfg.useMotion,
        cfg.useEdges
    );

    faceDetector = new FaceDetector(
        cfg.perceptualWidth,
        cfg.perceptualHeight,
        cfg.useFaces
    );

    frameProcessor = new FrameProcessor(
        cfg.encodeWidth,
        cfg.encodeHeight
    );

    privacyMask = new PrivacyMask(cfg.privacyZones);
}

Compressor::~Compressor()
{
    stop();

    delete importanceGen;
    delete faceDetector;
    delete frameProcessor;
    delete privacyMask;
}


// Start compressor

bool Compressor::start(const std::string& outputPath)
{
    if (running)
        return false;

    running = true;

    std::string cmd = buildFFmpegCommand(
        outputPath,
        cfg.encodeWidth,
        cfg.encodeHeight,
        cfg.crf,
        cfg.mode
    );

#ifdef _WIN32
    ffmpegPipe = _popen(cmd.c_str(), "wb");
#else
    ffmpegPipe = popen(cmd.c_str(), "w");
#endif

    if (!ffmpegPipe)
    {
        logError("Failed to open FFmpeg pipe");
        running = false;
        return false;
    }

    processingThread = std::thread(&Compressor::processingLoop, this);
    encodingThread = std::thread(&Compressor::encodingLoop, this);

    logInfo("Started asynchronous compressor");
    return true;
}

 
// Stop compressor
 
void Compressor::stop()
{
    if (!running)
        return;

    running = false;
    queueCV.notify_all();

    if (processingThread.joinable())
        processingThread.join();

    if (encodingThread.joinable())
        encodingThread.join();

    if (ffmpegPipe)
    {
#ifdef _WIN32
        _pclose(ffmpegPipe);
#else
        pclose(ffmpegPipe);
#endif
        ffmpegPipe = nullptr;
    }

    logInfo("Stopped compressor");
}

 
// Frame queueing
 
void Compressor::pushFrame(const FrameInfo& frame)
{
    enqueueFrame(frame.frame.clone());
}

void Compressor::enqueueFrame(const cv::Mat& frame)
{
    std::lock_guard<std::mutex> lock(queueMutex);
    frameQueue.push(frame);
    queueCV.notify_one();
}

cv::Mat Compressor::dequeueFrame()
{
    std::unique_lock<std::mutex> lock(queueMutex);

    queueCV.wait(lock, [&] {
        return !running || !frameQueue.empty();
        });

    if (frameQueue.empty())
        return cv::Mat();

    cv::Mat f = frameQueue.front();
    frameQueue.pop();
    return f;
}

 
// Processing thread
 
void Compressor::processingLoop()
{
    logInfo("Processing thread started");

    while (running)
    {
        cv::Mat raw = dequeueFrame();
        if (raw.empty())
            continue;

        cv::Mat small;
        cv::resize(raw, small, cv::Size(cfg.perceptualWidth, cfg.perceptualHeight));

        ImportanceMap faceMap = faceDetector->detect(small);
        ImportanceMap imp = importanceGen->compute(small, faceMap);

        privacyMask->apply(raw);

        cv::Mat processed = frameProcessor->process(raw, imp);

        enqueueFrame(processed);
    }

    logInfo("Processing thread exiting");
}

 
// Encoding thread
 
void Compressor::encodingLoop()
{
    logInfo("Encoding thread started");

    while (running || !frameQueue.empty())
    {
        cv::Mat frame = dequeueFrame();
        if (frame.empty())
            continue;

        fwrite(frame.data, 1, frame.total() * frame.elemSize(), ffmpegPipe);
    }

    logInfo("Encoding thread exiting");
}
