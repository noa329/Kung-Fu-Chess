#pragma once

#include <string>
#include <optional>

struct RestDurations {
    long long longRestMs;
    long long shortRestMs;
};

/**
 * Computes the real long_rest/short_rest cooldown durations
 * (frame_count / frames_per_sec * 1000, rounded) from one representative
 * piece's sprite config, reusing SpriteAnimation's own config-parsing/frame
 * loading rather than re-deriving it. See
 * docs/tasks/config-driven-rest-durations.md for the full rationale.
 *
 * Returns std::nullopt if the representative piece's short_rest/long_rest
 * sprite folders are missing or unreadable, so the caller can fall back to
 * RealTimeArbiter's own built-in defaults instead of crashing.
 *
 * @param assetsRoot         path to the kungfu-graphics folder
 * @param pieceSet           sprite set in use, e.g. "pieces1" or "pieces2"
 * @param representativePiece sprite-folder code to read timing from, e.g. "PW"
 */
std::optional<RestDurations> computeRestDurationsFromSprites(
    const std::string& assetsRoot,
    const std::string& pieceSet,
    const std::string& representativePiece);
