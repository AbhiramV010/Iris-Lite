#include "logging.hpp"
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>

bool LOG_DEBUG_MODE = false;

static std::string getCurrentTimestamp()
{
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);

    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

void logInfo(const std::string& msg)
{
    std::cout << "[" << getCurrentTimestamp() << "] [INFO] " << msg << std::endl;
}

void logWarn(const std::string& msg)
{
    std::cerr << "[" << getCurrentTimestamp() << "] [WARN] " << msg << std::endl;
}

void logError(const std::string& msg)
{
    std::cerr << "[" << getCurrentTimestamp() << "] [ERROR] " << msg << std::endl;
}

void logDebug(const std::string& msg)
{
    if (LOG_DEBUG_MODE)
        std::cout << "[" << getCurrentTimestamp() << "] [DEBUG] " << msg << std::endl;
}