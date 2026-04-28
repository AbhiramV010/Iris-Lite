#pragma once

#include <string>
#include <cstdint>

class StorageManager
{
public:
    std::string buildPath(uint64_t startFrame,
                          uint64_t endFrame);
};