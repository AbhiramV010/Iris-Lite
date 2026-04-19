#include "ImportanceMap.hpp"

ImportanceMap::ImportanceMap()
    : global(0.0f)
{
}

void ImportanceMap::update(const ImportanceSignal& r)
{
    float score =
        0.5f * r.motion +
        0.3f * r.edges +
        0.2f * r.faces;

    float alpha = 0.2f;

    global = (1.0f - alpha) * global + alpha * score;
}

void ImportanceMap::decay(float rate)
{
    global *= rate;
}

float ImportanceMap::getGlobal() const
{
    return global;
}