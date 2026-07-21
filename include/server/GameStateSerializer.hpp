#ifndef SERVER_GAME_STATE_SERIALIZER_H
#define SERVER_GAME_STATE_SERIALIZER_H
#include "GameEngine.hpp"
#include <string>

// Turns a GameSnapshot into the JSON string broadcast to clients over the
// WebSocket connection. Deliberately a REDUCED field set, additive
// alongside GameSnapshot rather than a replacement for it - the local
// graphics renderer keeps pulling the full GameSnapshot directly at 60fps
// via GameEngine::snapshot(), unaffected by anything here.
//
// Included: board tokens, cellStates, scores, move history, gameOver/result.
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
    std::string serialize(const GameSnapshot& snapshot);
}
#endif
