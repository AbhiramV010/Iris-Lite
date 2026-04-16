#include <opencv2/opencv.hpp>
#include <iostream>
#include <thread>
#include <chrono>

#include "Config.hpp"
#include "Compressor.hpp"
#include "logging.hpp"

int main(int argc, char* argv[])
{
    logInfo("IRIS-Lite Compression Engine starting");

    Config cfg;
    cfg.encodeWidth = 1280;
    cfg.encodeHeight = 720;
    cfg.perceptualWidth = 320;
    cfg.perceptualHeight = 180;
    cfg.useFaces = false;
    cfg.useMotion = true;
    cfg.useEdges = true;
    cfg.crf = 24;
    cfg.mode = "pi_hw";

    CompressionEngine engine(cfg);

    if (!engine.initialize())
    {
        logError("Failed to initialize compression engine");
        return -1;
    }

    cv::VideoCapture cap(0);
    if (!cap.isOpened())
    {
        logError("Failed to open camera");
        return -1;
    }

    cap.set(cv::CAP_PROP_FRAME_WIDTH, 1280);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, 720);

    uint64_t frameIndex = 0;
    cv::Mat frame;

    std::thread engineThread([&engine]()
        {
            engine.run();
        });

    logInfo("Frame capture loop starting");

    while (true)
    {
        if (!cap.read(frame))
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        engine.pushFrame(frame, frameIndex++);

        if (cv::waitKey(1) == 'q')
            break;
    }

    engine.shutdown();

    if (engineThread.joinable())
        engineThread.join();

    cap.release();

    logInfo("Compression engine exited cleanly");
    return 0;
}