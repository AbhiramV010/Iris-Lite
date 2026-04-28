#pragma once
#include <cstddef>

// 5 minutes @ 24 FPS
constexpr size_t FRAME_BUFFER_SIZE = 24 * 60 * 5;
constexpr size_t SLOT_SIZE = 80000;
static constexpr uint64_t FRAME_BUFFER_SIZE = 300;
static constexpr uint64_t SLOT_SIZE = 1 << 20;