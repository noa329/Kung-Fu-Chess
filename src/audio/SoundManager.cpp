// This is the sole translation unit that compiles miniaudio's implementation
// (the single-header-library convention: exactly one .cpp defines
// MA_IMPLEMENTATION before including the header). Every other file that
// needs audio goes through SoundManager.hpp instead of touching miniaudio.h
// directly.
#define MA_IMPLEMENTATION
#include "miniaudio.h"

#include "SoundManager.hpp"
#include <iostream>
#include <unordered_map>

struct SoundManager::Impl {
    ma_engine engine{};
    bool engineReady = false;
    bool enabled = true;
    std::string soundsRoot = "assets/sounds";

    // Long-lived, never-started ma_sound handles whose only job is to hold a
    // reference on each SFX's resource-manager data buffer so it stays
    // decoded in memory after the first play instead of being unloaded (and
    // re-decoded from disk) between moves. Actual playback goes through
    // ma_engine_play_sound(), which creates its own independent, overlapping
    // instance sharing that same cached data.
    //
    // Stored as heap-allocated unique_ptrs, not ma_sound-by-value: miniaudio
    // sounds are self-referential once initialized (internal pointers refer
    // back to the object's own address, and async loading jobs capture that
    // address too), so an initialized ma_sound must never be copied or moved
    // - it has to be constructed once at its final, permanent address.
    std::unordered_map<std::string, std::unique_ptr<ma_sound>> sfxCache;

    ma_sound musicSound{};
    bool musicReady = false;

    std::string resolve(const std::string& relativePath) const {
        if (soundsRoot.empty()) return relativePath;
        return soundsRoot + "/" + relativePath;
    }
};

SoundManager& SoundManager::instance() {
    static SoundManager instance;
    return instance;
}

SoundManager::SoundManager() : impl_(std::make_unique<Impl>()) {
    ma_engine_config config = ma_engine_config_init();
    ma_result result = ma_engine_init(&config, &impl_->engine);
    if (result != MA_SUCCESS) {
        std::cerr << "[SoundManager] Warning: failed to initialize audio engine ("
                  << ma_result_description(result) << "). Continuing with sound disabled."
                  << std::endl;
        impl_->engineReady = false;
        return;
    }
    impl_->engineReady = true;
}

SoundManager::~SoundManager() {
    if (!impl_) return;
    stopMusic();
    for (auto& entry : impl_->sfxCache) {
        ma_sound_uninit(entry.second.get());
    }
    if (impl_->engineReady) {
        ma_engine_uninit(&impl_->engine);
    }
}

void SoundManager::setSoundsRoot(const std::string& path) {
    impl_->soundsRoot = path;
}

void SoundManager::setEnabled(bool enabled) {
    impl_->enabled = enabled;
}

bool SoundManager::isEnabled() const {
    return impl_->enabled;
}

void SoundManager::toggleEnabled() {
    setEnabled(!impl_->enabled);
}

void SoundManager::playSound(const std::string& relativePath) {
    if (!impl_->engineReady || !impl_->enabled) return;

    std::string fullPath = impl_->resolve(relativePath);

    // Warm the cache the first time this path is played: an ma_sound that's
    // never started/stopped, kept alive for the program's lifetime purely to
    // hold the resource manager's reference count above zero so the decoded
    // PCM data isn't discarded between moves. Init is async (MA_SOUND_FLAG_ASYNC),
    // so this never blocks even on the very first play of a given sound.
    if (impl_->sfxCache.find(relativePath) == impl_->sfxCache.end()) {
        auto cached = std::make_unique<ma_sound>();
        ma_result cacheResult = ma_sound_init_from_file(
            &impl_->engine, fullPath.c_str(),
            MA_SOUND_FLAG_DECODE | MA_SOUND_FLAG_ASYNC, nullptr, nullptr, cached.get());
        if (cacheResult == MA_SUCCESS) {
            impl_->sfxCache.emplace(relativePath, std::move(cached));
        } else {
            std::cerr << "[SoundManager] Warning: failed to load sound \"" << fullPath
                      << "\" (" << ma_result_description(cacheResult)
                      << "). Continuing without sound for this event." << std::endl;
            return;
        }
    }

    // Fire-and-forget playback: creates its own independent instance, mixed
    // by the engine alongside any other sounds already playing, sharing the
    // cached decode above rather than re-reading the file from disk.
    ma_result result = ma_engine_play_sound(&impl_->engine, fullPath.c_str(), nullptr);
    if (result != MA_SUCCESS) {
        std::cerr << "[SoundManager] Warning: failed to play sound \"" << fullPath
                  << "\" (" << ma_result_description(result) << ")." << std::endl;
    }
}

void SoundManager::playMoveSound() {
    playSound("move.wav");
}

void SoundManager::playCaptureSound() {
    playSound("capture.wav");
}

void SoundManager::playJumpSound() {
    playSound("jump.wav");
}

void SoundManager::playMusic(const std::string& relativePath, float volume) {
    if (!impl_->engineReady) return;

    stopMusic();

    std::string fullPath = impl_->resolve(relativePath);
    ma_result result = ma_sound_init_from_file(
        &impl_->engine, fullPath.c_str(),
        MA_SOUND_FLAG_STREAM | MA_SOUND_FLAG_ASYNC, nullptr, nullptr, &impl_->musicSound);
    if (result != MA_SUCCESS) {
        std::cerr << "[SoundManager] Warning: failed to load music \"" << fullPath
                  << "\" (" << ma_result_description(result)
                  << "). Continuing without background music." << std::endl;
        return;
    }

    impl_->musicReady = true;
    ma_sound_set_looping(&impl_->musicSound, MA_TRUE);
    ma_sound_set_volume(&impl_->musicSound, volume);

    ma_result startResult = ma_sound_start(&impl_->musicSound);
    if (startResult != MA_SUCCESS) {
        std::cerr << "[SoundManager] Warning: failed to start music \"" << fullPath
                  << "\" (" << ma_result_description(startResult) << ")." << std::endl;
    }
}

void SoundManager::stopMusic() {
    if (!impl_->musicReady) return;
    ma_sound_stop(&impl_->musicSound);
    ma_sound_uninit(&impl_->musicSound);
    impl_->musicReady = false;
}
