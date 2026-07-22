#ifndef SERVER_GAME_SESSION_H
#define SERVER_GAME_SESSION_H
#include "GameEngine.hpp"
#include "GameCommandParser.hpp"
#include "Logger.hpp"
#include <string>

// Result of GameSession::handleCommand - covers only the validation this
// class is responsible for (is there a piece at the claimed square, does
// its color/letter match the command). Anything past that (shape/path/
// timing legality: illegal shape, blocked path, resting piece, pending
// move) is GameEngine's own existing silent-no-op behavior via
// select()/jump() - exactly the same as an illegal click through
// Controller today. handleCommand does not, and cannot, report those as
// errors, since GameEngine's public API has no return value for them.
struct CommandResult {
    bool ok;
    std::string error; // set only when ok == false, e.g. "ERROR PIECE_MISMATCH"
};

// Task B2: player-slot color assignment. First successful call gets
// White, second gets Black. A third is rejected - structurally
// unreachable in practice (Task D3: a GameSession is only ever created
// for exactly two already-matched players - see WebSocketServer's
// handleMatch()), but assignSeat() stays a safe, defined decision either
// way rather than relying on that upstream guarantee, same defensive-
// validation reasoning GameCommandParser's error taxonomy already
// established.
//
// Task D3: authentication is no longer part of this at all.
// GameSession::handleJoin(username, password) (Task C3) is gone -
// authentication now happens exactly once, at login time, directly via
// WebSocketServer's own AuthService, *before* any GameSession exists to
// call it on (matchmaking is what decides which two already-authenticated
// usernames end up sharing a session, and only then is one created). So
// GameSession no longer knows about AuthService/passwords at all - see
// assignSeat() below, which takes only an already-authenticated username.
struct JoinResult {
    bool ok;
    char color;       // 'w' | 'b', meaningful only when ok == true
    bool hasOpponent; // true when the other color slot was already filled
                      // before this call (i.e. this is the 2nd successful
                      // seat assignment)
    std::string error; // set only when ok == false, e.g. "ERROR SESSION_FULL"
};

// Owns one GameEngine (one concurrent game = one GameSession = one
// isolated GameEngine + EventBus, per the event_bus design). Turns an
// already-parsed ParsedCommand (Task A2) into the corresponding
// GameEngine calls. Connection/socket handling is a later task (A5) -
// this class deliberately knows nothing about networking yet.
//
// Subscribes to engine_.events() (onMoveLogged/onScoreUpdated/
// onGameLifecycle/onSound) in the constructor, always - but every
// subscription is gated on logger_ being non-null, so a default-
// constructed GameSession (used throughout the existing test suite)
// stays silent. attachLogger() lets the composition root (WebSocketServer)
// opt a real session into logging without needing a second constructor
// overload or breaking GameSession()'s existing default-constructibility.
// Logger itself is owned externally (WebSocketServer, shared across every
// session since SessionManager/Task D1) - not per-session, so multiple
// concurrent sessions can log to one shared file.
class GameSession {
    GameEngine engine_;
    Logger* logger_ = nullptr;
    bool whiteJoined_ = false;
    bool blackJoined_ = false;
    std::string whiteUsername_;
    std::string blackUsername_;
    // Task D4: per-seat connection status. Defaults true - a seat only
    // ever becomes disconnected via an explicit markDisconnected() call
    // from WebSocketServer's close handler, never implicitly. Deliberately
    // plain bools, not a shared "ConnectionState" enum: this is
    // server-layer connection bookkeeping, not one of the board/animation
    // states the project's strings-not-enums convention actually governs.
    bool whiteConnected_ = true;
    bool blackConnected_ = true;

    void logEvent(const std::string& message);

public:
    GameSession();

    GameEngine& engine() { return engine_; }
    void attachLogger(Logger& logger) { logger_ = &logger; }

    CommandResult handleCommand(const ParsedCommand& cmd);
    // Assigns a color to an already-authenticated username - see the
    // class comment above for why authentication itself isn't this
    // class's job (or even reachable from here) any more.
    JoinResult assignSeat(const std::string& username);

    // Task D4: which seat (if any) a username occupies in this session -
    // '\0' if it's neither whiteUsername_ nor blackUsername_. Lets
    // WebSocketServer turn "this hdl closed" (which it only knows a
    // username for, via its own authenticatedUsers_) into "this session's
    // white/black seat just disconnected" without GameSession needing to
    // know about connection_hdl at all.
    char colorOf(const std::string& username) const;
    // The username occupying `color`'s seat, or "" if that seat isn't
    // filled yet. Used by WebSocketServer's reconnect ack to report the
    // opponent's name the same way handleMatch()'s original "joined"
    // message does.
    std::string usernameFor(char color) const;

    void markDisconnected(char color);
    void markReconnected(char color);
    bool isConnected(char color) const;
};
#endif
