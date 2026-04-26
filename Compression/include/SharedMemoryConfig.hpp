#pragma once

#include <cstddef>

static constexpr int FPS = 24;
static constexpr int BUFFER_MINUTES = 5;
static constexpr size_t FRAME_BUFFER_SIZE = FPS * 60 * BUFFER_MINUTES;
static constexpr size_t SLOT_SIZE = 150000;

static constexpr int AUDIO_RATE = 44100;
static constexpr size_t AUDIO_CHUNK_SIZE = AUDIO_RATE / FPS;
static constexpr size_t AUDIO_SLOT_SIZE = AUDIO_CHUNK_SIZE * 2;
