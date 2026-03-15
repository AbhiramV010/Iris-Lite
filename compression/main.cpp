#include <iostream>
#include <thread>
#include <chrono>
#include <filesystem>

#include "compressor.hpp"
#include "file_watcher.hpp"
#include "utils.hpp"

// This file handles the main loop of the system.
// The idea is pretty simple:
// - Check the "raw" folder every few seconds
// - If new videos show up, compress them
// - Delete the raw files after compression (privacy)
// - Keep doing this forever while the Pi is running

int main() {
    std::string rawFolder = "raw/";
    std::string outFolder = "compressed/";

    FileWatcher watcher;
    Compressor compressor;

    std::cout << "Video compressor is running...\n";

    while (true) {
        // Look for new .mp4 files in the raw folder
        auto videos = watcher.getNewVideos(rawFolder);

        for (const auto& file : videos) {
            std::string inputPath = rawFolder + file;

            // Add a suffix so we don't overwrite anything
            std::string outputPath = outFolder + Utils::replaceExtension(file, "_compressed.mp4");

            std::cout << "Found new video: " << inputPath << "\n";
            std::cout << "Compressing to: " << outputPath << "\n";

            bool ok = compressor.compressVideo(inputPath, outputPath);

            if (!ok) {
                std::cout << "Something went wrong while compressing.\n";
            }
            else {
                std::cout << "Finished processing: " << file << "\n";
            }
        }

        // Wait a bit before checking again
        std::this_thread::sleep_for(std::chrono::seconds(10));
    }

    return 0;
}
