#include "Piece_animator.hpp"

namespace {
    // Every piece folder is expected to have these five state subfolders
    // (matches kungfu-graphics/pieces1 and pieces2).
    const char* kKnownStates[] = { "idle", "move", "jump", "short_rest", "long_rest" };
}

bool PieceAnimator::init(const std::string& piece_dir, const std::pair<int, int>& frame_size) {
    states_.clear();
    bool any_loaded = false;

    for (const char* state_name : kKnownStates) {
        SpriteAnimation anim;
        const std::string state_dir = piece_dir + "/states/" + state_name;
        if (anim.load(state_dir, frame_size)) {
            states_[state_name] = std::move(anim);
            any_loaded = true;
        }
    }

    current_state_ = "idle";
    return any_loaded && states_.count("idle") > 0;
}

void PieceAnimator::update(int dt_ms) {
    auto it = states_.find(current_state_);
    if (it == states_.end()) {
        return;
    }

    bool finished = it->second.update(dt_ms);
    if (finished) {
        const std::string next = it->second.nextState();
        setState(states_.count(next) ? next : "idle");
    }
}

void PieceAnimator::setState(const std::string& state_name) {
    auto it = states_.find(state_name);
    if (it == states_.end()) {
        return; // unknown state - keep whatever is currently playing
    }
    current_state_ = state_name;
    it->second.reset();
}

const Img& PieceAnimator::currentFrame() const {
    return states_.at(current_state_).currentFrame();
}