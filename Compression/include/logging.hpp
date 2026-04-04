#pragma once
#include <string>

// Toggle debug mode globally
extern bool LOG_DEBUG_MODE;

// Logging functions
void logInfo(const std::string& msg);
void logWarn(const std::string& msg);
void logError(const std::string& msg);
void logDebug(const std::string& msg);
