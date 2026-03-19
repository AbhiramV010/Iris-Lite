#include "logging.hpp"
#include <iostream>
#include <fstream>
#include <mutex>

namespace {
    std::mutex logMutex;
    std::ofstream logFile("logs/compressor.log", std::ios::app);
}

void logInfo(const std::string& msg) {
    std::lock_guard<std::mutex> lock(logMutex);
    std::cout << "[INFO] " << msg << '\n';
    if (logFile) logFile << "[INFO] " << msg << '\n';
}

void logError(const std::string& msg) {
    std::lock_guard<std::mutex> lock(logMutex);
    std::cerr << "[ERROR] " << msg << '\n';
    if (logFile) logFile << "[ERROR] " << msg << '\n';
}
