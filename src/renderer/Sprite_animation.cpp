#include "Sprite_animation.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <regex>
#include <iostream>

namespace fs = std::filesystem;

bool SpriteAnimation::load(const std::string& state_dir, const std::pair<int, int>& frame_size) {
    frames_.clear();
    elapsed_ms_ = 0;
    current_index_ = 0;

    const std::string sprites_dir = state_dir + "/sprites";
    if (!fs::exists(sprites_dir)) {
        return false;
    }

    // Frames are named 1.png, 2.png, ... - load them in that order.
    for (int i = 1; ; ++i) {
        std::string frame_path = sprites_dir + "/" + std::to_string(i) + ".png";
        if (!fs::exists(frame_path)) {
            break;
        }
        Img frame;
        frame.read(frame_path, frame_size, false);
        frames_.push_back(std::move(frame));
    }

    parseConfig(state_dir + "/config.json");

    return !frames_.empty();
}

void SpriteAnimation::parseConfig(const std::string& config_path) {
    // Defaults if the file is missing or a field can't be found.
    fps_ = 6;
    is_loop_ = true;
    next_state_ = "idle";

    std::ifstream in(config_path);
    if (!in.is_open()) {
        return;
    }
    std::stringstream buffer;
    buffer << in.rdbuf();
    std::string content = buffer.str();

    std::smatch match;

    static const std::regex fps_re(R"re("frames_per_sec"\s*:\s*([0-9]+))re");
    if (std::regex_search(content, match, fps_re) && match.size() > 1) {
        fps_ = std::max(1, std::stoi(match[1].str()));
    }

    static const std::regex loop_re(R"re("is_loop"\s*:\s*(true|false))re");
    if (std::regex_search(content, match, loop_re) && match.size() > 1) {
        is_loop_ = (match[1].str() == "true");
    }

    static const std::regex next_re(R"re("next_state_when_finished"\s*:\s*"([A-Za-z_]+)")re");
    if (std::regex_search(content, match, next_re) && match.size() > 1) {
        next_state_ = match[1].str();
    }
}

void SpriteAnimation::reset() {
    elapsed_ms_ = 0;
    current_index_ = 0;
}

bool SpriteAnimation::update(int dt_ms) {
    if (frames_.empty()) {
        return false;
    }

    elapsed_ms_ += dt_ms;
    const int frame_duration_ms = 1000 / fps_;
    size_t idx = static_cast<size_t>(elapsed_ms_ / frame_duration_ms);

    if (is_loop_) {
        current_index_ = idx % frames_.size();
        return false;
    }

    if (idx >= frames_.size() - 1) {
        current_index_ = frames_.size() - 1;
        return true; // finished playing a one-shot animation
    }

    current_index_ = idx;
    return false;
}

const Img& SpriteAnimation::currentFrame() const {
    return frames_.at(current_index_);
}