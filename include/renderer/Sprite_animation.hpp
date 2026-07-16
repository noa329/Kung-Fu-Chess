#pragma once

#include <string>
#include <vector>
#include "img.hpp"

/**
 * Loads and plays a single animation "state" (e.g. idle / move / jump /
 * short_rest / long_rest) for one piece: the numbered sprite frames
 * (sprites/1.png, 2.png, ...) plus the timing/looping info from that
 * state's config.json.
 *
 * This class only reads image data through Img and never uses any other
 * graphics library.
 */
class SpriteAnimation {
public:
    SpriteAnimation() = default;

    /**
     * Load every frame from state_dir/sprites/N.png (numbered 1..n) and the
     * timing info from state_dir/config.json.
     *
     * @param state_dir  path to a single state folder, e.g. ".../PW/states/idle"
     * @param frame_size target (width, height) to resize every frame to (cell size)
     * @return true on success (at least one frame loaded)
     */
    bool load(const std::string& state_dir, const std::pair<int, int>& frame_size);

    /** Restart this animation from its first frame. */
    void reset();

    /**
     * Advance the animation by dt_ms milliseconds.
     * @return true the moment a non-looping animation has played its last
     *         frame (the caller should then switch to nextState()).
     */
    bool update(int dt_ms);

    const Img& currentFrame() const;

    int fps() const { return fps_; }
    bool isLoop() const { return is_loop_; }
    const std::string& nextState() const { return next_state_; }
    size_t frameCount() const { return frames_.size(); }
    bool isLoaded() const { return !frames_.empty(); }

private:
    std::vector<Img> frames_;
    int fps_ = 6;
    bool is_loop_ = true;
    std::string next_state_ = "idle";

    int elapsed_ms_ = 0;
    size_t current_index_ = 0;

    void parseConfig(const std::string& config_path);
};