#pragma once

// Unified importance data contract for ALL modules.
// This replaces scattered interpretation of "importance".

struct ImportanceSignal
{
    // Core perceptual signals
    float motion = 0.0f;
    float edges = 0.0f;
    float faces = 0.0f;

    // Aggregated importance score (0–1 normalized)
    float global = 0.0f;

    // Stability / confidence of detection (future fusion layer)
    float confidence = 1.0f;
};