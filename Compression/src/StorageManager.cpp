#include "StorageManager.hpp"
#include <sys/stat.h>
#include <sstream>
#include <ctime>

static const std::string BASE = "/mnt/clipDrive/clips/";

void StorageManager::ensureReady()
{
    mkdir("/mnt/clipDrive", 0777);
    mkdir(BASE.c_str(), 0777);
}

std::string StorageManager::buildPath(uint64_t start,
                                      uint64_t end,
                                      const std::string& tag)
{
    std::ostringstream ss;

    std::time_t t = std::time(nullptr);

    ss << BASE << "iris_"
       << t << "_"
       << start << "_"
       << end << "_"
       << tag << ".mp4";

    return ss.str();
}
