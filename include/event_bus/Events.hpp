#ifndef EVENT_BUS_EVENTS_H
#define EVENT_BUS_EVENTS_H
#include <string>

// Fired whenever a capture changes a player's score.
struct ScoreUpdatedEvent {
    char color;    // 'w' | 'b' - the side whose score just changed
    int newScore;  // that side's total score after this update
    int delta;     // points just gained (the captured piece's value)
};

// Fired once per logged move/jump, mirroring GameEngine::MoveRecord.
struct MoveLoggedEvent {
    long long atMs;
    char color;
    std::string notation;
};

// Fired for each discrete sound cue. `name` matches an asset file under
// assets/sounds/<name>.wav (e.g. "move", "capture", "jump") - kept as a
// string rather than an enum to match this project's existing
// strings-not-enums convention for identifiers consumed as data lookups.
struct SoundEvent {
    std::string name;
};

// Fired once at game start and once at game end (for one-shot animation/UI
// triggers) - not a substitute for GameSnapshot::gameOver, which stays the
// per-frame read-model for continuous rendering.
struct GameLifecycleEvent {
    std::string phase;  // "start" | "end"
    std::string result; // meaningful only when phase == "end": "White Wins" | "Black Wins" | "Draw"
};

#endif
