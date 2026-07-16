#pragma once

#include <string>
#include <map>
#include "img.hpp"
#include "Sprite_animation.hpp"

/**
 * Owns every animation state (idle / move / jump / short_rest / long_rest)
 * for a single piece instance and drives the state machine between them:
 * each state's config.json says whether it loops and, if not, which state
 * to switch to once it finishes (see SpriteAnimation).
 */
class PieceAnimator {
public:
    /**
     * @param piece_dir  path to a piece's folder, e.g. ".../pieces1/PW"
     *                   (must contain a "states" subfolder)
     * @param frame_size target (width, height) to resize every sprite to (cell size)
     */
    bool init(const std::string& piece_dir, const std::pair<int, int>& frame_size);

    /** Advance the current state's animation and follow state transitions. */
    void update(int dt_ms);

    /** Force a state change (e.g. "move" after a click), restarting it from frame 0. */
    void setState(const std::string& state_name);

    const std::string& state() const { return current_state_; }
    const Img& currentFrame() const;

private:
    std::map<std::string, SpriteAnimation> states_;
    std::string current_state_ = "idle";
};