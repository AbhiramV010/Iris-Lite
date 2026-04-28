#pragma once

#include <cstdint>

#ifndef FRAME_BUFFER_SIZE
static constexpr std::uint64_t FRAME_BUFFER_SIZE = 24ULL * 60ULL * 5ULL;
#endif

#ifndef SLOT_SIZE
static constexpr std::uint64_t SLOT_SIZE = 1ULL << 20;
#endif