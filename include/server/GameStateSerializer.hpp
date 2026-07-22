#ifndef SERVER_GAME_STATE_SERIALIZER_H
#define SERVER_GAME_STATE_SERIALIZER_H
#include "GameEngine.hpp"
#include <optional>
#include <string>

// Turns a GameSnapshot into the JSON string broadcast to clients over the
// WebSocket connection. Deliberately a REDUCED field set, additive
// alongside GameSnapshot rather than a replacement for it - the local
// graphics renderer keeps pulling the full GameSnapshot directly at 60fps
// via GameEngine::snapshot(), unaffected by anything here.
//
// Included: board tokens, cellStates, scores, move history, gameOver/result,
// per-color disconnect countdown (Task D4, present only while that seat is
// actually disconnected).
//
// Deliberately excluded (design decision, confirmed before this task
// started):
// - moveTargets/moveProgress/selected: per-frame interpolation/render-loop
//   state, meaningless without a continuously-ticking client render loop.
// - captureFlashes: same category as the above. The text-only shell
//   client (Phase B) reads state via BoardPrinter, which has nothing to
//   show a capture flash with. Revisit only if/when the graphics binary
//   gets network support - a separate, unscoped future task.
//
// Uses nlohmann::json internally, but that type never appears in this
// header - callers just get a plain string to send as a WS text frame.
namespace GameStateSerializer {

// Task D4: per-color disconnect countdown, live only while that seat is
// disconnected. Deliberately NOT part of GameSnapshot/GameEngine - which
// color's connection is currently down is connection-layer state
// (WebSocketServer/GameSession), not game state, so it doesn't belong on
// the read-model GameEngine hands to the local (connection-less) graphics
// renderer too. Milliseconds remaining, not an absolute deadline
// timestamp: client and server wall clocks aren't synchronized anywhere
// else in this protocol, and WebSocketServer already recomputes real
// elapsed time from steady_clock every tick (same pattern as the D3
// matchmaking timeout), so handing over "how much is left" sidesteps
// clock-sync entirely.
struct DisconnectStatus {
    std::optional<int> whiteRemainingMs;
    std::optional<int> blackRemainingMs;
};

std::string serialize(const GameSnapshot& snapshot, const DisconnectStatus& disconnect = {});
}
#endif
