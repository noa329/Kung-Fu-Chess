#include "doctest.h"
#include "GameSession.hpp"

// server layer: GameSession owns one GameEngine and turns an already-parsed
// ParsedCommand (see GameCommandParser, Task A2) into GameEngine calls.
// Validates the claimed color/piece letter against what's actually on the
// board at the origin square (fail on mismatch) - anything past that
// (shape/path/timing legality) is GameEngine's own silent-no-op behavior,
// unchanged, same as a click through Controller today.

TEST_CASE("a correctly-matched move command schedules and resolves the move") {
    GameSession session;
    session.engine().loadBoard({{"wR", ".", "."}});

    ParsedCommand cmd{false, 'w', 'R', Position{0, 0}, Position{0, 2}};
    auto result = session.handleCommand(cmd);
    REQUIRE(result.ok);

    session.engine().wait(2000);
    auto snap = session.engine().snapshot();
    CHECK(snap.boardTokens == std::vector<std::vector<std::string>>{{".", ".", "wR"}});
}

TEST_CASE("a correctly-matched jump command schedules the jump") {
    GameSession session;
    session.engine().loadBoard({{"wR", ".", "."}});

    ParsedCommand cmd{true, 'w', 'R', Position{0, 0}, Position{}};
    auto result = session.handleCommand(cmd);
    REQUIRE(result.ok);

    auto snap = session.engine().snapshot();
    CHECK(snap.cellStates[0][0] == "jump");
}

TEST_CASE("a command naming an empty square is rejected without mutating engine state") {
    GameSession session;
    session.engine().loadBoard({{"wR", ".", "."}});

    ParsedCommand cmd{false, 'w', 'R', Position{0, 1}, Position{0, 2}};
    auto result = session.handleCommand(cmd);
    CHECK(result.ok == false);
    CHECK(result.error == "ERROR NO_PIECE_AT_SQUARE");

    auto snap = session.engine().snapshot();
    CHECK(snap.boardTokens == std::vector<std::vector<std::string>>{{"wR", ".", "."}});
    CHECK(snap.selected == Position{-1, -1});
}

TEST_CASE("a command with a mismatched piece letter is rejected without mutating engine state") {
    GameSession session;
    session.engine().loadBoard({{"wR", ".", "."}});

    ParsedCommand cmd{false, 'w', 'Q', Position{0, 0}, Position{0, 2}};
    auto result = session.handleCommand(cmd);
    CHECK(result.ok == false);
    CHECK(result.error == "ERROR PIECE_MISMATCH");

    auto snap = session.engine().snapshot();
    CHECK(snap.boardTokens == std::vector<std::vector<std::string>>{{"wR", ".", "."}});
    CHECK(snap.selected == Position{-1, -1});
}

TEST_CASE("a command with a mismatched color is rejected without mutating engine state") {
    GameSession session;
    session.engine().loadBoard({{"wR", ".", "."}});

    ParsedCommand cmd{false, 'b', 'R', Position{0, 0}, Position{0, 2}};
    auto result = session.handleCommand(cmd);
    CHECK(result.ok == false);
    CHECK(result.error == "ERROR COLOR_MISMATCH");

    auto snap = session.engine().snapshot();
    CHECK(snap.boardTokens == std::vector<std::vector<std::string>>{{"wR", ".", "."}});
    CHECK(snap.selected == Position{-1, -1});
}

TEST_CASE("a command whose origin square is out of bounds is rejected") {
    GameSession session;
    session.engine().loadBoard({{"wR", ".", "."}});

    ParsedCommand cmd{false, 'w', 'R', Position{99, 99}, Position{0, 2}};
    auto result = session.handleCommand(cmd);
    CHECK(result.ok == false);
    CHECK(result.error == "ERROR NO_PIECE_AT_SQUARE");
}

TEST_CASE("an illegal-shape move is accepted by validation but silently not scheduled, like a Controller click") {
    // Same square is a degenerate/no-op destination - GameEngine's own
    // select()/select() already treats this as "nothing to do", same as
    // clicking a piece then clicking its own square via Controller.
    GameSession session;
    session.engine().loadBoard({{"wR", ".", "."}});

    ParsedCommand cmd{false, 'w', 'R', Position{0, 0}, Position{0, 0}};
    auto result = session.handleCommand(cmd);
    CHECK(result.ok == true); // validation passed - GameEngine silently declines the move itself

    session.engine().wait(2000);
    auto snap = session.engine().snapshot();
    CHECK(snap.boardTokens == std::vector<std::vector<std::string>>{{"wR", ".", "."}});
}
