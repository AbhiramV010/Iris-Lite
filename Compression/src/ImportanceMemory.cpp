#include "ImportanceMemory.hpp"
#include <algorithm>

void ImportanceMemory::update(const std::string& trigger, float score, bool wasUseful)
{
    float& bias = biasTable[trigger];

    float target = wasUseful ? 1.0f : 0.0f;

    float error = target - score;

    // adaptive learning rate
    float lr = wasUseful ? 0.08f : 0.04f;

    bias += lr * error;

    bias = std::clamp(bias, -0.6f, 0.6f);
}

float ImportanceMemory::getBias(const std::string& trigger) const
{
    auto it = biasTable.find(trigger);
    return it == biasTable.end() ? 0.0f : it->second;
}