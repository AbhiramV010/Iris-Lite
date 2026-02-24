#define _CRT_SECURE_NO_WARNINGS
#include "utils.hpp"
#include <ctime>

std::string generateFilename() {
    time_t t = time(nullptr);
    char buf[64];
    strftime(buf, sizeof(buf), "output/event_%Y%m%d_%H%M%S.mp4", localtime(&t));
    return std::string(buf);
}
