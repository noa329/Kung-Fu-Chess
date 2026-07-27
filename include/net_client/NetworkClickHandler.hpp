#ifndef NET_CLIENT_NETWORK_CLICK_HANDLER_H
#define NET_CLIENT_NETWORK_CLICK_HANDLER_H
#include "GameEngine.hpp"
#include <optional>
#include <string>

// docs/tasks/graphics-networked-client-plan.md, Task H4: the pure half of
// networked click handling. Everything except the pixel->Position
// conversion and the actual socket send stays here - both of those need
// pixels/OpenCV/OnlineClient, so they live in the graphics-only
// NetworkController (kungfu-graphics/cpp/src/) instead. This class only
// ever sees already-converted board Positions and a GameSnapshot, so it's
// fully doctest-covered without a window or a live connection - matches
// the plan's own stated test for this task ("given a cached board + a
// scripted click sequence, confirm the exact wire string built").
//
// Mirrors GameEngine::select()'s own two-click gesture as closely as a
// client that can't see full movement-shape legality can:
//   - A first click on a piece of the caller's own color starts a pending
//     selection. Unlike GameEngine::select()'s own first click (which
//     accepts *either* color, since local play has no "which side is the
//     local player" concept - both colors share one screen/mouse), this
//     filters by `myColor` from the very first click, since online play
//     very much does have that concept.
//   - A second click on another same-color piece re-selects (mirrors
//     GameEngine::select() switching `selected` instead of attempting an
//     illegal same-color "capture").
//   - A second click on an empty square or an opponent's piece builds and
//     returns a move/capture command, clearing the pending selection
//     either way. Unlike GameEngine::select(), which *keeps* the selection
//     when the attempted move turns out to be shape-illegal, this class has
//     no way to know that - shape/path legality is server-side only
//     (GameSession::handleCommand(), Task E2 is the sole authority) - so it
//     always clears rather than guessing.
//   - If the piece at the pending "from" square has moved/been captured by
//     the time the second click arrives (real-time travel-time moves make
//     this a real scenario, not a hypothetical - the board isn't
//     turn-locked), the click is silently dropped rather than building a
//     command from stale data.
class NetworkClickHandler {
    char myColor_;
    Position pendingFrom_{-1, -1};
    bool hasPending_ = false;

public:
    explicit NetworkClickHandler(char myColor) : myColor_(myColor) {}

    // Returns the wire command to send ("WQe2e5"-shaped), or nullopt if
    // this click didn't produce one (started/changed a pending selection,
    // or was ignored outright - an out-of-bounds click, an empty/opponent
    // square on a first click, or a now-stale pending selection).
    std::optional<std::string> handleClick(const GameSnapshot& snapshot, const Position& pos);

    // A single-click jump ("JWPe2"-shaped) - no pending-selection state
    // involved, mirrors Controller::handleJump()'s one-click-one-attempt
    // shape. Only sends for a click on one of the caller's own pieces.
    std::optional<std::string> handleJump(const GameSnapshot& snapshot, const Position& pos) const;

    // For the render loop's click-highlight reconstruction: GameSnapshot::
    // selected is never sent over the wire (Task H2, decision 1) - this is
    // the client's own best-effort substitute, reflecting only *this*
    // client's own pending click, not any other player's.
    bool hasPendingSelection() const { return hasPending_; }
    Position pendingSelection() const { return pendingFrom_; }
};

#endif
