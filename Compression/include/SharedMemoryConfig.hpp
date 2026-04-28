#pragma once

#include <cstdint>

// 5-minute circular buffer @ 24fps
static constexpr uint64_t FRAME_BUFFER_SIZE = 24 * 60 * 5;
static constexpr uint64_t SLOT_SIZE = 1 << 20;