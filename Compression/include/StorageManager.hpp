#pragma once

#include <string>
#include <cstdint>

class StorageManager
{
public:
    void ensureReady();

    std::string buildPath(uint64_t start,
                          uint64_t end,
                          const std::string& tag = "");
};