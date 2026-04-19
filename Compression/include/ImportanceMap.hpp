#pragma once

#include "ImportanceSignal.hpp"

class ImportanceMap
{
public:
    ImportanceMap();

    void update(const ImportanceSignal& r);
    void decay(float rate);
    float getGlobal() const;

private:
    float global = 0.0f;
};