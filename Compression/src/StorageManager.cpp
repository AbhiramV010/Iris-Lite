#include "StorageManager.hpp"

#include <sys/stat.h>
#include <sstream>
#include <cstdint>

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

    ss << BASE
       << "iris_"
       << start << "_"
       << end;

    if (!tag.empty())
        ss << "_" << tag;

    ss << ".mp4";

    return ss.str();
}