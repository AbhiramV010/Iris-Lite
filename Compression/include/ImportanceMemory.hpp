#pragma once
#include <unordered_map>
#include <string>

class ImportanceMemory
{
public:
    void update(const std::string& trigger, float score, bool wasUseful);

    float getBias(const std::string& trigger) const;

private:
    std::unordered_map<std::string, float> biasTable;
};