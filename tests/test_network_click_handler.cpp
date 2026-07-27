#include "doctest.h"
#include "NetworkClickHandler.hpp"

// net_client layer: NetworkClickHandler is the pure half of Task H4's
// networked click handling - given a cached board (GameSnapshot) and a
// scripted sequence of already-converted board Positions, confirms the
// exact wire command string built (or that nothing is built), without any
// pixels/OpenCV/socket involved at all.

namespace {

GameSnapshot standardStartSnapshot() {
    GameSnapshot snap{};
    snap.boardTokens = {
        {"bR", "bN", "bB", "bQ", "bK", "bB", "bN", "bR"},
        {"bP", "bP", "bP", "bP", "bP", "bP", "bP", "bP"},
        {".", ".", ".", ".", ".", ".", ".", "."},
        {".", ".", ".", ".", ".", ".", ".", "."},
        {".", ".", ".", ".", ".", ".", ".", "."},
        {".", ".", ".", ".", ".", ".", ".", "."},
        {"wP", "wP", "wP", "wP", "wP", "wP", "wP", "wP"},
        {"wR", "wN", "wB", "wQ", "wK", "wB", "wN", "wR"},
    };
    return snap;
}

} // namespace

TEST_CASE("first click on the caller's own piece starts a pending selection") {
    NetworkClickHandler handler('w');
    auto snap = standardStartSnapshot();

    auto command = handler.handleClick(snap, Position{6, 4}); // e2, a white pawn

    CHECK(!command.has_value());
    CHECK(handler.hasPendingSelection());
    CHECK(handler.pendingSelection() == Position{6, 4});
}

TEST_CASE("first click on an empty square is ignored") {
    NetworkClickHandler handler('w');
    auto snap = standardStartSnapshot();

    auto command = handler.handleClick(snap, Position{4, 4}); // e4, empty

    CHECK(!command.has_value());
    CHECK(!handler.hasPendingSelection());
}

TEST_CASE("first click on an opponent's piece is ignored - color mismatch rejected before sending") {
    NetworkClickHandler handler('w');
    auto snap = standardStartSnapshot();

    auto command = handler.handleClick(snap, Position{1, 4}); // e7, a black pawn

    CHECK(!command.has_value());
    CHECK(!handler.hasPendingSelection());
}

TEST_CASE("second click on an empty square builds a move command and clears the pending selection") {
    NetworkClickHandler handler('w');
    auto snap = standardStartSnapshot();

    handler.handleClick(snap, Position{6, 4}); // e2
    auto command = handler.handleClick(snap, Position{4, 4}); // e4

    REQUIRE(command.has_value());
    CHECK(*command == "WPe2e4");
    CHECK(!handler.hasPendingSelection());
}

TEST_CASE("second click on an opponent's piece builds a capture command") {
    // Custom fixture: a black pawn placed where a white pawn could
    // plausibly capture - this handler doesn't check movement-shape
    // legality at all (that's server-side only), so any opponent-occupied
    // target builds a command, legal-looking or not.
    auto snap = standardStartSnapshot();
    snap.boardTokens[5][3] = "bP"; // d3

    NetworkClickHandler handler('w');
    handler.handleClick(snap, Position{6, 4}); // e2
    auto command = handler.handleClick(snap, Position{5, 3}); // d3

    REQUIRE(command.has_value());
    CHECK(*command == "WPe2d3");
    CHECK(!handler.hasPendingSelection());
}

TEST_CASE("second click on another same-color piece re-selects instead of building a command") {
    NetworkClickHandler handler('w');
    auto snap = standardStartSnapshot();

    handler.handleClick(snap, Position{6, 4}); // e2
    auto command = handler.handleClick(snap, Position{6, 3}); // d2, also a white pawn

    CHECK(!command.has_value());
    CHECK(handler.hasPendingSelection());
    CHECK(handler.pendingSelection() == Position{6, 3});
}

TEST_CASE("second click drops a stale pending selection instead of building a command from it") {
    // The pending piece is no longer where it was clicked (moved/captured
    // since the first click) - a real scenario in a real-time game with no
    // turn lock, not a hypothetical.
    auto snap = standardStartSnapshot();

    NetworkClickHandler handler('w');
    handler.handleClick(snap, Position{6, 4}); // e2, pending

    auto staleSnap = snap;
    staleSnap.boardTokens[6][4] = "."; // e2 emptied out from under the pending selection

    auto command = handler.handleClick(staleSnap, Position{4, 4});

    CHECK(!command.has_value());
    CHECK(!handler.hasPendingSelection());
}

TEST_CASE("an out-of-bounds click is ignored, not a crash") {
    NetworkClickHandler handler('w');
    auto snap = standardStartSnapshot();

    CHECK(!handler.handleClick(snap, Position{-1, -1}).has_value());
    CHECK(!handler.handleClick(snap, Position{99, 99}).has_value());
    CHECK(!handler.hasPendingSelection());
}

TEST_CASE("black's color letter is used correctly in a built command") {
    NetworkClickHandler handler('b');
    auto snap = standardStartSnapshot();

    handler.handleClick(snap, Position{1, 4}); // e7, a black pawn
    auto command = handler.handleClick(snap, Position{3, 4}); // e5

    REQUIRE(command.has_value());
    CHECK(*command == "BPe7e5");
}

TEST_CASE("handleJump builds a jump command for the caller's own piece") {
    NetworkClickHandler handler('w');
    auto snap = standardStartSnapshot();

    auto command = handler.handleJump(snap, Position{7, 1}); // b1, a white knight

    REQUIRE(command.has_value());
    CHECK(*command == "JWNb1");
}

TEST_CASE("handleJump is ignored for an empty square or an opponent's piece") {
    NetworkClickHandler handler('w');
    auto snap = standardStartSnapshot();

    CHECK(!handler.handleJump(snap, Position{4, 4}).has_value());  // e4, empty
    CHECK(!handler.handleJump(snap, Position{1, 4}).has_value());  // e7, black pawn
}
