#include "logging.hpp"
#include <iostream>

void logInfo(const std::string& s) { std::cout << "[INFO] " << s << "\n"; }
void logWarn(const std::string& s) { std::cerr << "[WARN] " << s << "\n"; }
void logError(const std::string& s) { std::cerr << "[ERR ] " << s << "\n"; }