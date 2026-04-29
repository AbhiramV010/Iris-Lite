#include "ImportanceMemory.hpp"
#include <algorithm>

void ImportanceMemory::update(const std::string& trigger, float score, bool wasUseful)
{
    float& bias = biasTable[trigger];

    float target = wasUseful ? 1.0f : 0.0f;

    float error = target - score;

    bias += 0.05f * error;

    bias = std::clamp(bias, -0.5f, 0.5f);
}

float ImportanceMemory::getBias(const std::string& trigger) const
{
    auto it = biasTable.find(trigger);
    return it == biasTable.end() ? 0.0f : it->second;
}