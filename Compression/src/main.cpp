#include <opencv2/opencv.hpp>
#include "Config.hpp"
#include "Compressor.hpp"
#include "Utils.hpp"
#include "logging.hpp"
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>

// JSON for metadata
#include "../external/json.hpp"
using json = nlohmann::json;

namespace fs = std::filesystem;

int main()
{
    // Root directory: wherever the binary is run from
    const std::string ROOT = std::filesystem::current_path().string();

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

    // Optional camera preview (not required for compression)
    cv::VideoCapture cam(0);
    if (!cam.isOpened())
    {
        logWarn("Camera not available. Detection preview disabled.");
    }
    else
    {
        logInfo("Camera detection preview started");
    }

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
        std::string baseName = fs::path(file).stem().string();
        std::string outName = outputDir + "/" + baseName + "_compressed.mp4";
        std::string metaName = outputDir + "/" + baseName + "_compressed.json";

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
        int frameCount = 0;
        while (cap.read(frame))
        {
            if (frame.empty()) continue;

            FrameInfo info;
            info.frame = frame;
            compressor.pushFrame(info);
            frameCount++;
        }

        cap.release();
        compressor.stop();
        logInfo("Finished compressing: " + file + " (" + std::to_string(frameCount) + " frames)");

        // Write simple metadata sidecar
        try
        {
            json meta;
            meta["input_file"] = file;
            meta["output_file"] = outName;
            meta["frames"] = frameCount;
            meta["encode_width"] = cfg.encodeWidth;
            meta["encode_height"] = cfg.encodeHeight;
            meta["perceptual_width"] = cfg.perceptualWidth;
            meta["perceptual_height"] = cfg.perceptualHeight;
            meta["crf"] = cfg.crf;
            meta["mode"] = cfg.mode;

            std::ofstream metaOut(metaName);
            metaOut << meta.dump(4);
            logInfo("Wrote metadata: " + metaName);
        }
        catch (const std::exception& e)
        {
            logError(std::string("Failed to write metadata: ") + e.what());
        }

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
