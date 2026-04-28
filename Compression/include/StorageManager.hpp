#pragma once
#include <string>

class StorageManager
{
public:
    std::string buildPath(uint64_t startFrame,
                          uint64_t endFrame,
                          const std::string& tag);

    void ensureReady();
};
