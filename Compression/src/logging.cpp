#include "logging.hpp"
#include <iostream>
#include <fstream>
#include <mutex>
#include <chrono>
#include <filesystem>

bool LOG_DEBUG_MODE = false;  // You can enable this in main()

namespace {
    std::mutex logMutex;
    std::ofstream logFile;

    // ANSI colors
    const char* GREEN = "\033[32m";
    const char* YELLOW = "\033[33m";
    const char* RED = "\033[31m";
    const char* BLUE = "\033[34m";
    const char* RESET = "\033[0m";

    std::string timestamp() {
        using namespace std::chrono;
        auto now = system_clock::now();
        auto t = system_clock::to_time_t(now);

        char buf[32];
        strftime(buf, sizeof(buf), "%H:%M:%S", localtime(&t));
        return std::string(buf);
    }

    void initLogFile() {
        std::filesystem::create_directories("logs");
        logFile.open("logs/compressor.log", std::ios::app);
    }

    struct LogInit {
        LogInit() { initLogFile(); }
    } _logInit;
}

void logInfo(const std::string& msg) {
    std::lock_guard<std::mutex> lock(logMutex);
    std::cout << GREEN << "[INFO " << timestamp() << "] " << RESET << msg << '\n';
    if (logFile) logFile << "[INFO " << timestamp() << "] " << msg << '\n';
}

void logWarn(const std::string& msg) {
    std::lock_guard<std::mutex> lock(logMutex);
    std::cout << YELLOW << "[WARN " << timestamp() << "] " << RESET << msg << '\n';
    if (logFile) logFile << "[WARN " << timestamp() << "] " << msg << '\n';
}

void logError(const std::string& msg) {
    std::lock_guard<std::mutex> lock(logMutex);
    std::cerr << RED << "[ERROR " << timestamp() << "] " << RESET << msg << '\n';
    if (logFile) logFile << "[ERROR " << timestamp() << "] " << msg << '\n';
}

void logDebug(const std::string& msg) {
    if (!LOG_DEBUG_MODE) return;  // Silent unless enabled

    std::lock_guard<std::mutex> lock(logMutex);
    std::cout << BLUE << "[DEBUG " << timestamp() << "] " << RESET << msg << '\n';
    if (logFile) logFile << "[DEBUG " << timestamp() << "] " << msg << '\n';
}
