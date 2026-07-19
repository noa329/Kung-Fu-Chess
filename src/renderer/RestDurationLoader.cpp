#include "RestDurationLoader.hpp"
#include "Sprite_animation.hpp"
#include <cmath>

namespace {

// SpriteAnimation::load() both loads the numbered sprite frames and parses
// the state's config.json (frames_per_sec) - reusing it here means this
// duration computation is driven by exactly the same frame count and fps
// the graphics view itself uses to play the animation, with no separate
// config-parsing logic to keep in sync.
bool durationFromState(const std::string& assetsRoot, const std::string& pieceSet,
                        const std::string& representativePiece, const std::string& state,
                        long long& outMs) {
    SpriteAnimation anim;
    const std::string stateDir = assetsRoot + "/" + pieceSet + "/" + representativePiece +
                                  "/states/" + state;
    if (!anim.load(stateDir, {0, 0}) || anim.fps() <= 0) {
        return false;
    }
    outMs = static_cast<long long>(std::llround(
        static_cast<double>(anim.frameCount()) / anim.fps() * 1000.0));
    return true;
}

} // namespace

std::optional<RestDurations> computeRestDurationsFromSprites(
    const std::string& assetsRoot, const std::string& pieceSet,
    const std::string& representativePiece) {
    RestDurations result{};
    if (!durationFromState(assetsRoot, pieceSet, representativePiece, "long_rest", result.longRestMs)) {
        return std::nullopt;
    }
    if (!durationFromState(assetsRoot, pieceSet, representativePiece, "short_rest", result.shortRestMs)) {
        return std::nullopt;
    }
    return result;
}
