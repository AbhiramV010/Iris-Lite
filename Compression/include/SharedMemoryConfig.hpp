#pragma once
#include <cstdint>

static constexpr std::uint64_t FRAME_WIDTH = 1920;
static constexpr std::uint64_t FRAME_HEIGHT = 1080;
static constexpr std::uint64_t FRAME_CHANNELS = 3;

static constexpr std::uint64_t FRAME_SIZE =
FRAME_WIDTH * FRAME_HEIGHT * FRAME_CHANNELS;

// SINGLE FRAME ONLY 
static constexpr std::uint64_t SHM_SIZE = FRAME_SIZE;