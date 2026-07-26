#include "doctest.h"
#include "GameSession.hpp"
#include <sstream>

// server layer: GameSession owns one GameEngine and turns an already-parsed
// ParsedCommand (see GameCommandParser, Task A2) into GameEngine calls.
// Validates the claimed color/piece letter against what's actually on the
// board at the origin square (fail on mismatch) - anything past that
// (shape/path/timing legality) is GameEngine's own silent-no-op behavior,
// unchanged, same as a click through Controller today.
//
// Task E2: handleCommand() now takes the sender's username too, and
// authorizes by their actual assigned color (join()'d once, not re-derived
// from the command) before any board-state check - see GameSession.hpp's
// comment on handleCommand() for the full reasoning. Every test below that
// isn't specifically testing that authorization join()s a matching-color
// username first, so it reaches the same board-level check it always did.

TEST_CASE("a correctly-matched move command schedules and resolves the move") {
    GameSession session;
    session.engine().loadBoard({{"wR", ".", "."}});
    session.join("Alice"); // White

    ParsedCommand cmd{false, 'w', 'R', Position{0, 0}, Position{0, 2}};
    auto result = session.handleCommand("Alice", cmd);
    REQUIRE(result.ok);

    session.engine().wait(2000);
    auto snap = session.engine().snapshot();
    CHECK(snap.boardTokens == std::vector<std::vector<std::string>>{{".", ".", "wR"}});
}

TEST_CASE("a correctly-matched jump command schedules the jump") {
    GameSession session;
    session.engine().loadBoard({{"wR", ".", "."}});
    session.join("Alice"); // White

    ParsedCommand cmd{true, 'w', 'R', Position{0, 0}, Position{}};
    auto result = session.handleCommand("Alice", cmd);
    REQUIRE(result.ok);

    auto snap = session.engine().snapshot();
    CHECK(snap.cellStates[0][0] == "jump");
}

TEST_CASE("a command naming an empty square is rejected without mutating engine state") {
    GameSession session;
    session.engine().loadBoard({{"wR", ".", "."}});
    session.join("Alice"); // White

    ParsedCommand cmd{false, 'w', 'R', Position{0, 1}, Position{0, 2}};
    auto result = session.handleCommand("Alice", cmd);
    CHECK(result.ok == false);
    CHECK(result.error == "ERROR NO_PIECE_AT_SQUARE");

    auto snap = session.engine().snapshot();
    CHECK(snap.boardTokens == std::vector<std::vector<std::string>>{{"wR", ".", "."}});
    CHECK(snap.selected == Position{-1, -1});
}

TEST_CASE("a command with a mismatched piece letter is rejected without mutating engine state") {
    GameSession session;
    session.engine().loadBoard({{"wR", ".", "."}});
    session.join("Alice"); // White

    ParsedCommand cmd{false, 'w', 'Q', Position{0, 0}, Position{0, 2}};
    auto result = session.handleCommand("Alice", cmd);
    CHECK(result.ok == false);
    CHECK(result.error == "ERROR PIECE_MISMATCH");

    auto snap = session.engine().snapshot();
    CHECK(snap.boardTokens == std::vector<std::vector<std::string>>{{"wR", ".", "."}});
    CHECK(snap.selected == Position{-1, -1});
}

TEST_CASE("a command with a mismatched color is rejected without mutating engine state") {
    GameSession session;
    session.engine().loadBoard({{"wR", ".", "."}});
    session.join("Alice"); // White
    session.join("Bob");   // Black

    // Bob (Black) legitimately claims Black in the command - authorization
    // passes - but the board square he's naming actually holds a white
    // rook, so this is still rejected, just at the board-check stage.
    ParsedCommand cmd{false, 'b', 'R', Position{0, 0}, Position{0, 2}};
    auto result = session.handleCommand("Bob", cmd);
    CHECK(result.ok == false);
    CHECK(result.error == "ERROR COLOR_MISMATCH");

    auto snap = session.engine().snapshot();
    CHECK(snap.boardTokens == std::vector<std::vector<std::string>>{{"wR", ".", "."}});
    CHECK(snap.selected == Position{-1, -1});
}

TEST_CASE("a command whose origin square is out of bounds is rejected") {
    GameSession session;
    session.engine().loadBoard({{"wR", ".", "."}});
    session.join("Alice"); // White

    ParsedCommand cmd{false, 'w', 'R', Position{99, 99}, Position{0, 2}};
    auto result = session.handleCommand("Alice", cmd);
    CHECK(result.ok == false);
    CHECK(result.error == "ERROR NO_PIECE_AT_SQUARE");
}

TEST_CASE("an illegal-shape move is accepted by validation but silently not scheduled, like a Controller click") {
    // Same square is a degenerate/no-op destination - GameEngine's own
    // select()/select() already treats this as "nothing to do", same as
    // clicking a piece then clicking its own square via Controller.
    GameSession session;
    session.engine().loadBoard({{"wR", ".", "."}});
    session.join("Alice"); // White

    ParsedCommand cmd{false, 'w', 'R', Position{0, 0}, Position{0, 0}};
    auto result = session.handleCommand("Alice", cmd);
    CHECK(result.ok == true); // validation passed - GameEngine silently declines the move itself

    session.engine().wait(2000);
    auto snap = session.engine().snapshot();
    CHECK(snap.boardTokens == std::vector<std::vector<std::string>>{{"wR", ".", "."}});
}

// Task E2: identity-based authorization itself - the point of this task
// is that a spectator or an impersonating player is rejected by *who they
// are*, not just by whether their claimed color happens to match the
// board (which any connection can satisfy by simply claiming whichever
// color is actually on the square).

TEST_CASE("a spectator's command is rejected without mutating engine state") {
    GameSession session;
    session.engine().loadBoard({{"wR", ".", "."}});
    session.join("Alice"); // White
    session.join("Bob");   // Black
    session.join("Carol"); // spectator

    ParsedCommand cmd{false, 'w', 'R', Position{0, 0}, Position{0, 2}};
    auto result = session.handleCommand("Carol", cmd);
    CHECK(result.ok == false);
    CHECK(result.error == "ERROR NOT_A_PLAYER");

    auto snap = session.engine().snapshot();
    CHECK(snap.boardTokens == std::vector<std::vector<std::string>>{{"wR", ".", "."}});
}

TEST_CASE("a command from a username that never joined this session is rejected the same way") {
    GameSession session;
    session.engine().loadBoard({{"wR", ".", "."}});
    session.join("Alice"); // White

    ParsedCommand cmd{false, 'w', 'R', Position{0, 0}, Position{0, 2}};
    auto result = session.handleCommand("Eve", cmd);
    CHECK(result.ok == false);
    CHECK(result.error == "ERROR NOT_A_PLAYER");
}

TEST_CASE("a player claiming the opponent's color is rejected by identity, not just the board") {
    GameSession session;
    session.engine().loadBoard({{"wR", "bN", "."}});
    session.join("Alice"); // White
    session.join("Bob");   // Black

    // Bob's connection (Black) sends a command CLAIMING White's rook - the
    // claimed color even matches what's genuinely on that square, so the
    // old board-only check would have let this straight through. Only an
    // identity check catches it.
    ParsedCommand cmd{false, 'w', 'R', Position{0, 0}, Position{0, 2}};
    auto result = session.handleCommand("Bob", cmd);
    CHECK(result.ok == false);
    CHECK(result.error == "ERROR NOT_YOUR_COLOR");

    auto snap = session.engine().snapshot();
    CHECK(snap.boardTokens == std::vector<std::vector<std::string>>{{"wR", "bN", "."}}); // untouched
}

TEST_CASE("each player can only command their own color") {
    GameSession session;
    session.engine().loadBoard({{"wR", "bN", "."}});
    session.join("Alice"); // White
    session.join("Bob");   // Black

    ParsedCommand whiteMove{false, 'w', 'R', Position{0, 0}, Position{0, 0}};
    CHECK(session.handleCommand("Alice", whiteMove).ok == true);

    ParsedCommand blackMove{false, 'b', 'N', Position{0, 1}, Position{0, 1}};
    CHECK(session.handleCommand("Bob", blackMove).ok == true);

    // Cross-claims are both rejected.
    CHECK(session.handleCommand("Alice", blackMove).ok == false);
    CHECK(session.handleCommand("Bob", whiteMove).ok == false);
}

// Task G3: voluntary resign - no claimed color on the wire, colorOf(username)
// alone decides who (if anyone) this resigns.

TEST_CASE("handleResign ends the game in favor of the other color, either color resigning") {
    GameSession whiteResigns;
    whiteResigns.engine().loadBoard({{"wR", "bN", "."}});
    whiteResigns.join("Alice"); // White
    whiteResigns.join("Bob");   // Black
    CHECK(whiteResigns.handleResign("Alice").ok == true);
    CHECK(whiteResigns.engine().snapshot().gameOver == true);
    CHECK(whiteResigns.engine().snapshot().result == "Black Wins");

    GameSession blackResigns;
    blackResigns.engine().loadBoard({{"wR", "bN", "."}});
    blackResigns.join("Alice"); // White
    blackResigns.join("Bob");   // Black
    CHECK(blackResigns.handleResign("Bob").ok == true);
    CHECK(blackResigns.engine().snapshot().gameOver == true);
    CHECK(blackResigns.engine().snapshot().result == "White Wins");
}

TEST_CASE("handleResign fires the lifecycle end event, same as a king capture would") {
    std::ostringstream sink;
    Logger logger({&sink});
    GameSession session;
    session.attachLogger(logger);
    session.engine().loadBoard({{"wR", "bN", "."}});
    session.join("Alice");
    session.join("Bob");

    session.handleResign("Alice");
    CHECK(sink.str().find("lifecycle phase=end result=Black Wins") != std::string::npos);
}

TEST_CASE("handleResign rejects a spectator or unjoined username without ending the game") {
    GameSession session;
    session.engine().loadBoard({{"wR", "bN", "."}});
    session.join("Alice"); // White
    session.join("Bob");   // Black
    session.join("Carol"); // spectator (3rd join, Task E1)

    auto spectatorResult = session.handleResign("Carol");
    CHECK(spectatorResult.ok == false);
    CHECK(spectatorResult.error == "ERROR NOT_A_PLAYER");
    CHECK(session.engine().snapshot().gameOver == false);

    auto strangerResult = session.handleResign("NeverJoined");
    CHECK(strangerResult.ok == false);
    CHECK(strangerResult.error == "ERROR NOT_A_PLAYER");
    CHECK(session.engine().snapshot().gameOver == false);
}

TEST_CASE("handleResign on an already-finished game is a safe no-op") {
    GameSession session;
    session.engine().loadBoard({{"wR", "bN", "."}});
    session.join("Alice"); // White
    session.join("Bob");   // Black

    CHECK(session.handleResign("Alice").ok == true);
    CHECK(session.engine().snapshot().result == "Black Wins");

    // Bob "resigning" after Alice already did doesn't flip the result -
    // GameEngine::resign() itself no-ops once gameOver is already true.
    CHECK(session.handleResign("Bob").ok == true);
    CHECK(session.engine().snapshot().result == "Black Wins");
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
    session.join("Alice"); // White

    session.handleCommand("Alice", ParsedCommand{false, 'w', 'R', Position{0, 0}, Position{0, 2}});

    CHECK(sink.str().empty());
}

TEST_CASE("after attachLogger, a move logs color/notation") {
    std::ostringstream sink;
    Logger logger({&sink});
    GameSession session;
    session.attachLogger(logger);
    session.engine().loadBoard({{"wR", ".", "."}});
    session.join("Alice"); // White

    session.handleCommand("Alice", ParsedCommand{false, 'w', 'R', Position{0, 0}, Position{0, 2}});

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
    session.join("Alice"); // White

    session.handleCommand("Alice", ParsedCommand{true, 'w', 'R', Position{0, 0}, Position{}});

    CHECK(sink.str().find("sound name=jump") != std::string::npos);
}

TEST_CASE("after attachLogger, a resolved capture logs a score update") {
    std::ostringstream sink;
    Logger logger({&sink});
    GameSession session;
    session.attachLogger(logger);
    session.engine().loadBoard({{"wR", "bN", "."}});
    session.join("Alice"); // White

    session.handleCommand("Alice", ParsedCommand{false, 'w', 'R', Position{0, 0}, Position{0, 1}});
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
    session.join("Alice"); // White

    session.handleCommand("Alice", ParsedCommand{false, 'w', 'R', Position{0, 0}, Position{0, 1}});
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
// before any GameSession exists to call it on. join() takes only an
// already-authenticated username - see GameSession.hpp's class comment
// for the full reasoning. (AuthService's own accept/reject decision is
// tested independently in test_auth_service.cpp, unaffected by this.)

TEST_CASE("the first join is White with no opponent yet") {
    GameSession session;
    auto result = session.join("Alice");
    CHECK(result.ok);
    CHECK(result.color == 'w');
    CHECK(result.hasOpponent == false);
}

TEST_CASE("the second join is Black and reports the opponent is present") {
    GameSession session;
    session.join("Alice");
    auto result = session.join("Bob");
    CHECK(result.ok);
    CHECK(result.color == 'b');
    CHECK(result.hasOpponent == true);
}

// Task E1: a 3rd+ join is a spectator, not an error - both seats already
// being filled is exactly what makes a join the 3rd+ one, so hasOpponent
// is always true for a spectator result (see GameSession.hpp's comment).

TEST_CASE("a third join is a spectator, not a rejection") {
    GameSession session;
    session.join("Alice");
    session.join("Bob");
    auto result = session.join("Carol");
    CHECK(result.ok == true);
    CHECK(result.color == 's');
    CHECK(result.hasOpponent == true);
}

TEST_CASE("a fourth (and later) join is also a spectator") {
    GameSession session;
    session.join("Alice");
    session.join("Bob");
    session.join("Carol");
    auto result = session.join("Dave");
    CHECK(result.ok == true);
    CHECK(result.color == 's');
}

TEST_CASE("isSpectator reports true only for usernames that joined as spectators") {
    GameSession session;
    session.join("Alice");
    session.join("Bob");
    session.join("Carol");
    CHECK(session.isSpectator("Carol") == true);
    CHECK(session.isSpectator("Alice") == false); // White, not a spectator
    CHECK(session.isSpectator("Bob") == false);   // Black, not a spectator
    CHECK(session.isSpectator("Eve") == false);   // never joined at all
}

TEST_CASE("multiple spectators are each tracked independently") {
    GameSession session;
    session.join("Alice");
    session.join("Bob");
    session.join("Carol");
    session.join("Dave");
    CHECK(session.isSpectator("Carol") == true);
    CHECK(session.isSpectator("Dave") == true);
}

// Task D4: colorOf()/usernameFor() let WebSocketServer translate between
// "this hdl closed, and I only know its username" and "this session's
// white/black seat" without GameSession ever needing to know about
// connection_hdl - see GameSession.hpp's class comment.

TEST_CASE("colorOf reports the seat a known username occupies") {
    GameSession session;
    session.join("Alice");
    session.join("Bob");
    CHECK(session.colorOf("Alice") == 'w');
    CHECK(session.colorOf("Bob") == 'b');
}

TEST_CASE("colorOf reports the null char for an unknown username") {
    GameSession session;
    session.join("Alice");
    CHECK(session.colorOf("Carol") == '\0');
}

// Task E1: colorOf() deliberately does NOT distinguish "never joined" from
// "joined as a spectator" - both report '\0', since colorOf()'s only
// consumer only cares about resignable seats. isSpectator() is the query
// for telling those two cases apart (see GameSession.hpp's comment).

TEST_CASE("colorOf reports the null char for a spectator too, not a color") {
    GameSession session;
    session.join("Alice");
    session.join("Bob");
    session.join("Carol");
    CHECK(session.colorOf("Carol") == '\0');
    CHECK(session.isSpectator("Carol") == true); // proves it's the spectator case, not "unknown"
}

TEST_CASE("usernameFor reports the username occupying a seat, empty if unfilled") {
    GameSession session;
    session.join("Alice");
    CHECK(session.usernameFor('w') == "Alice");
    CHECK(session.usernameFor('b') == "");
}

// Task D4: per-seat connection status - defaults connected, flips on
// markDisconnected()/markReconnected(). Pure bookkeeping, no timers (the
// real 20s countdown timer lives in WebSocketServer, manually verified
// separately - same split D3's MatchmakingTimeout established).

TEST_CASE("a freshly assigned seat starts out connected") {
    GameSession session;
    session.join("Alice");
    CHECK(session.isConnected('w') == true);
}

TEST_CASE("markDisconnected/markReconnected flip only the given seat's status") {
    GameSession session;
    session.join("Alice");
    session.join("Bob");

    session.markDisconnected('w');
    CHECK(session.isConnected('w') == false);
    CHECK(session.isConnected('b') == true); // Bob's seat is untouched

    session.markReconnected('w');
    CHECK(session.isConnected('w') == true);
}
