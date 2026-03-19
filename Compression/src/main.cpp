#include <opencv2/opencv.hpp>
#include "Config.hpp"
#include "Compressor.hpp"
#include "Utils.hpp"
#include "logging.hpp"
#include <thread>
#include <chrono>
#include <filesystem>

namespace fs = std::filesystem;

int main()
{
    // *** FIXED ROOT DIRECTORY ***
    const std::string ROOT = "C:/Users/shrey/source/repos/Compressor";

    // Load config
    Config cfg;
    if (!loadConfig(cfg, ROOT + "/config/config.json"))
    {
        logError("Failed to load config.json");
        return -1;
    }

    // Build folder paths
    std::string inputDir = ROOT + "/input";
    std::string outputDir = ROOT + "/output";
    std::string failedDir = ROOT + "/failed";
    std::string logsDir = ROOT + "/logs";

    // Ensure required folders exist
    ensureDirectory(inputDir);
    ensureDirectory(outputDir);
    ensureDirectory(failedDir);
    ensureDirectory(logsDir);

    // Start camera for detection
    cv::VideoCapture cam(0);
    if (!cam.isOpened())
    {
        logError("Camera not available. Detection disabled.");
    }
    else
    {
        logInfo("Camera detection started");
    }

    // Background thread for detection
    std::thread detectionThread([&]() {
        if (!cam.isOpened()) return;

        cv::Mat frame;
        while (true)
        {
            if (!cam.read(frame)) continue;

            cv::imshow("Detection Preview", frame);
            if (cv::waitKey(1) == 27) break; // ESC to exit preview
        }
        });

    // Main loop: watch input folder
    while (true)
    {
        auto files = listVideoFiles(inputDir);

        if (files.empty())
        {
            std::this_thread::sleep_for(std::chrono::seconds(2));
            continue;
        }

        std::string file = files[0];
        logInfo("Processing file: " + file);

        cv::VideoCapture cap(file);
        if (!cap.isOpened())
        {
            logError("Failed to open video: " + file);

            std::string failedName = failedDir + "/" + fs::path(file).filename().string();
            moveFile(file, failedName);

            continue;
        }

        // Build output filename
        std::string outName =
            outputDir + "/" + fs::path(file).stem().string() + "_compressed.mp4";

        // Start compressor
        Compressor compressor(cfg);
        if (!compressor.start(outName))
        {
            logError("Failed to start compressor");

            std::string failedName = failedDir + "/" + fs::path(file).filename().string();
            moveFile(file, failedName);

            continue;
        }

        // Feed frames into compressor
        cv::Mat frame;
        while (cap.read(frame))
        {
            if (frame.empty()) continue;

            FrameInfo info;
            info.frame = frame;
            compressor.pushFrame(info);
        }

        // IMPORTANT: release the file so Windows can move it
        cap.release();

        compressor.stop();
        logInfo("Finished compressing: " + file);

        // Delete original file after successful compression
        std::error_code ec;
        fs::remove(file, ec);
        if (ec)
        {
            logError("Failed to delete original file: " + file + " (" + ec.message() + ")");
        }
        else
        {
            logInfo("Deleted original file: " + file);
        }

    }

    detectionThread.join();
    return 0;
}
