#include "doctest.h"
#include "GameSession.hpp"
#include <sstream>

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

// Task A6: GameSession subscribes engine_.events() to an optional Logger,
// gated so a default-constructed GameSession (used throughout every test
// above) stays silent unless attachLogger() is called - these tests are
// what confirm the gate actually works both ways.

TEST_CASE("without attachLogger, a move produces no log output") {
    std::ostringstream sink;
    Logger logger({&sink});
    GameSession session; // no attachLogger call
    session.engine().loadBoard({{"wR", ".", "."}});

    session.handleCommand(ParsedCommand{false, 'w', 'R', Position{0, 0}, Position{0, 2}});

    CHECK(sink.str().empty());
}

TEST_CASE("after attachLogger, a move logs color/notation") {
    std::ostringstream sink;
    Logger logger({&sink});
    GameSession session;
    session.attachLogger(logger);
    session.engine().loadBoard({{"wR", ".", "."}});

    session.handleCommand(ParsedCommand{false, 'w', 'R', Position{0, 0}, Position{0, 2}});

    std::string log = sink.str();
    CHECK(log.find("move") != std::string::npos);
    CHECK(log.find("color=w") != std::string::npos);
    CHECK(log.find("notation=a1c1") != std::string::npos); // single-row board, see GameCommandParser tests
}

TEST_CASE("after attachLogger, a jump logs a sound event") {
    std::ostringstream sink;
    Logger logger({&sink});
    GameSession session;
    session.attachLogger(logger);
    session.engine().loadBoard({{"wR", ".", "."}});

    session.handleCommand(ParsedCommand{true, 'w', 'R', Position{0, 0}, Position{}});

    CHECK(sink.str().find("sound name=jump") != std::string::npos);
}

TEST_CASE("after attachLogger, a resolved capture logs a score update") {
    std::ostringstream sink;
    Logger logger({&sink});
    GameSession session;
    session.attachLogger(logger);
    session.engine().loadBoard({{"wR", "bN", "."}});

    session.handleCommand(ParsedCommand{false, 'w', 'R', Position{0, 0}, Position{0, 1}});
    session.engine().wait(1000); // resolve the capture

    std::string log = sink.str();
    CHECK(log.find("score color=w") != std::string::npos);
    CHECK(log.find("delta=3") != std::string::npos); // knight
}

TEST_CASE("after attachLogger, a king capture logs a lifecycle end with the result") {
    std::ostringstream sink;
    Logger logger({&sink});
    GameSession session;
    session.attachLogger(logger);
    session.engine().loadBoard({{"wR", "bK"}});

    session.handleCommand(ParsedCommand{false, 'w', 'R', Position{0, 0}, Position{0, 1}});
    session.engine().wait(1000);

    std::string log = sink.str();
    CHECK(log.find("lifecycle phase=end") != std::string::npos);
    CHECK(log.find("result=White Wins") != std::string::npos);
}

// Task B2: join -> color assignment. Pure decision, decoupled from sockets/
// JSON - the wire-format parsing/dispatch and response framing that turn
// this into an actual `{"type":"join",...}` message live in
// WebSocketServer.cpp (networking glue, manually verified instead, same
// as the rest of A5).

TEST_CASE("the first join is assigned White with no opponent yet") {
    GameSession session;
    auto result = session.handleJoin("Alice");
    CHECK(result.ok);
    CHECK(result.color == 'w');
    CHECK(result.hasOpponent == false);
}

TEST_CASE("the second join is assigned Black and reports the opponent is present") {
    GameSession session;
    session.handleJoin("Alice");
    auto result = session.handleJoin("Bob");
    CHECK(result.ok);
    CHECK(result.color == 'b');
    CHECK(result.hasOpponent == true);
}

TEST_CASE("a third join is rejected") {
    GameSession session;
    session.handleJoin("Alice");
    session.handleJoin("Bob");
    auto result = session.handleJoin("Carol");
    CHECK(result.ok == false);
    CHECK(result.error == "ERROR SESSION_FULL");
}
