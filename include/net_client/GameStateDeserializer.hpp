#ifndef NET_CLIENT_GAME_STATE_DESERIALIZER_H
#define NET_CLIENT_GAME_STATE_DESERIALIZER_H
#include "GameEngine.hpp"
#include <optional>
#include <string>

// docs/tasks/graphics-networked-client-plan.md, Task H2: inverse of
// server/GameStateSerializer.hpp - turns one WebSocket text frame (the
// periodic, untyped state-tick broadcast) into a real GameSnapshot for the
// networked graphics client. Deliberately reuses GameSnapshot itself rather
// than a parallel wire-specific type (plan decision 1), so BoardView/HudView
// need zero code changes once Task H5 wires this in - same "reuse the
// existing type" call NetworkEventParser.hpp makes for SoundEvent/
// GameLifecycleEvent.
//
// Two GameSnapshot fields never round-trip through this message at all,
// deliberately defaulted rather than guessed at:
//   - captureFlashes: GameStateSerializer.hpp's own comment documents this
//     as excluded, render-loop-only decoration - still out of scope, see
//     docs/tasks/wire-protocol-move-progress-plan.md's own "Out of scope"
//     section.
//   - selected: transient server-side state scoped to a single
//     GameSession::handleCommand() call (two sequential
//     GameEngine::select() calls resolve one move), not persistent
//     per-connection "this player has square X highlighted" state - there
//     is nothing meaningful to recover here even in principle.
// Pieces will show no capture-flash effect and no click-highlight from
// this message alone, until/unless a later task revisits those.
//
// moveTargets/moveProgress (Task I2): populated from the wire's sparse
// "activeMoves" list (GameStateSerializer.hpp's own Task I1 addition) for
// exactly the cells it names; every other cell defaults to
// Position{-1,-1}/0.0 - the same sentinel this deserializer already used
// for "no data at all" before I2 existed. A server that predates I1, or a
// tick with nothing mid-move, leaves every cell at that same default, so
// this is a strict extension of the wire format, not a breaking change.
//
// whiteName/blackName are also left at their default-constructed empty
// string - this message never carries them (only the "joined"/room-flow
// messages do, a different message shape entirely, parsed elsewhere as
// part of Task H3b's login/menu flow) - left for whichever later task (H5)
// combines this snapshot with the names already known from login.
namespace GameStateDeserializer {

// std::nullopt for anything that isn't a state-broadcast frame: malformed
// JSON, or a well-formed but differently-shaped message (any "type"-tagged
// message on this connection - join/joined/sound/lifecycle/etc, none of
// which carry "board"). Uses the same has-a-"board"-key discriminator
// client/cli/main.cpp's own handleServerMessage() already relies on to tell
// a state broadcast apart from every typed message on the same connection.
std::optional<GameSnapshot> deserialize(const std::string& raw);

}
#endif
