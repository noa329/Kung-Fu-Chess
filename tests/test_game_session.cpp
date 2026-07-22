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

// Task B2: color assignment. Pure decision, decoupled from sockets/JSON -
// the wire-format parsing/dispatch and response framing that turns this
// into a real message live in WebSocketServer.cpp (networking glue,
// manually verified instead, same as the rest of A5).
//
// Task D3: authentication is no longer part of this at all - it happens
// once, at login time, directly via WebSocketServer's own AuthService,
// before any GameSession exists to call it on. assignSeat() takes only an
// already-authenticated username - see GameSession.hpp's class comment
// for the full reasoning. (AuthService's own accept/reject decision is
// tested independently in test_auth_service.cpp, unaffected by this.)

TEST_CASE("the first seat assigned is White with no opponent yet") {
    GameSession session;
    auto result = session.assignSeat("Alice");
    CHECK(result.ok);
    CHECK(result.color == 'w');
    CHECK(result.hasOpponent == false);
}

TEST_CASE("the second seat assigned is Black and reports the opponent is present") {
    GameSession session;
    session.assignSeat("Alice");
    auto result = session.assignSeat("Bob");
    CHECK(result.ok);
    CHECK(result.color == 'b');
    CHECK(result.hasOpponent == true);
}

TEST_CASE("a third seat assignment is rejected") {
    GameSession session;
    session.assignSeat("Alice");
    session.assignSeat("Bob");
    auto result = session.assignSeat("Carol");
    CHECK(result.ok == false);
    CHECK(result.error == "ERROR SESSION_FULL");
}

// Task D4: colorOf()/usernameFor() let WebSocketServer translate between
// "this hdl closed, and I only know its username" and "this session's
// white/black seat" without GameSession ever needing to know about
// connection_hdl - see GameSession.hpp's class comment.

TEST_CASE("colorOf reports the seat a known username occupies") {
    GameSession session;
    session.assignSeat("Alice");
    session.assignSeat("Bob");
    CHECK(session.colorOf("Alice") == 'w');
    CHECK(session.colorOf("Bob") == 'b');
}

TEST_CASE("colorOf reports the null char for an unknown username") {
    GameSession session;
    session.assignSeat("Alice");
    CHECK(session.colorOf("Carol") == '\0');
}

TEST_CASE("usernameFor reports the username occupying a seat, empty if unfilled") {
    GameSession session;
    session.assignSeat("Alice");
    CHECK(session.usernameFor('w') == "Alice");
    CHECK(session.usernameFor('b') == "");
}

// Task D4: per-seat connection status - defaults connected, flips on
// markDisconnected()/markReconnected(). Pure bookkeeping, no timers (the
// real 20s countdown timer lives in WebSocketServer, manually verified
// separately - same split D3's MatchmakingTimeout established).

TEST_CASE("a freshly assigned seat starts out connected") {
    GameSession session;
    session.assignSeat("Alice");
    CHECK(session.isConnected('w') == true);
}

TEST_CASE("markDisconnected/markReconnected flip only the given seat's status") {
    GameSession session;
    session.assignSeat("Alice");
    session.assignSeat("Bob");

    session.markDisconnected('w');
    CHECK(session.isConnected('w') == false);
    CHECK(session.isConnected('b') == true); // Bob's seat is untouched

    session.markReconnected('w');
    CHECK(session.isConnected('w') == true);
}
