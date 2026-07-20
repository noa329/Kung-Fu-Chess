#ifndef SOUND_MANAGER_H
#define SOUND_MANAGER_H
#include <string>
#include <memory>

// Singleton audio facade around miniaudio. Deliberately independent of Img
// and every graphics-layer type - GameEngine (and anything else in the
// engine layers) can call this without pulling in OpenCV or any rendering
// code. All playback is asynchronous: nothing here blocks the calling
// thread, so it's safe to call from inside RealTimeArbiter-driven code
// paths (GameEngine::select/jump/wait) without perturbing engine timing or
// the render loop's frame rate.
//
// A missing/corrupt sound file or a missing audio device never throws or
// crashes the game - failures are logged to stderr and playback for that
// event is silently skipped.
class SoundManager {
public:
    static SoundManager& instance();

    SoundManager(const SoundManager&) = delete;
    SoundManager& operator=(const SoundManager&) = delete;

    // Directory that playSound()/playMusic() relative paths are resolved
    // against. Defaults to "assets/sounds" (relative to the working
    // directory) until overridden.
    void setSoundsRoot(const std::string& path);

    // Gates playSound()/playMoveSound()/playCaptureSound()/playJumpSound() -
    // while disabled, those calls are skipped entirely. Music isn't touched
    // here; callers that want the mute key to also silence music should
    // pair this with an explicit stopMusic()/playMusic() call (see the
    // graphics main.cpp's 'M' key handler).
    void setEnabled(bool enabled);
    bool isEnabled() const;
    void toggleEnabled();

    // Short, cached sound effects. Multiple calls overlap/mix rather than
    // queuing - each is an independent, fire-and-forget playback.
    void playMoveSound();
    void playCaptureSound();
    void playJumpSound();
    void playSound(const std::string& relativePath);

    // Looping background music, streamed rather than fully decoded (it's
    // long-lived, unlike the short SFX above). Replaces any music already
    // playing.
    void playMusic(const std::string& relativePath, float volume = 1.0f);
    void stopMusic();

    ~SoundManager();

private:
    SoundManager();

    struct Impl;
    std::unique_ptr<Impl> impl_;
};
#endif
