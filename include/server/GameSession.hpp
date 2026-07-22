#ifndef SERVER_GAME_SESSION_H
#define SERVER_GAME_SESSION_H
#include "GameEngine.hpp"
#include "GameCommandParser.hpp"
#include "Logger.hpp"
#include "AuthService.hpp"
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

// Task B2: player-slot color assignment. First successful join gets White,
// second gets Black. A third join is rejected - structurally unreachable
// via a real client today (ConnectionRegistry, A5, already caps the
// session at 2 connections and closes a 3rd right after its handshake,
// before any message ever reaches here), but handleJoin() stays a safe,
// defined decision either way rather than relying on that upstream
// guarantee, same defensive-validation reasoning GameCommandParser's error
// taxonomy already established.
//
// Task C3: color assignment now happens only *after* AuthService accepts
// the username/password - see handleJoin() below. error covers both
// reasons now: "ERROR SESSION_FULL" (unchanged from B2) and whatever
// AuthService::authenticate() rejected with (e.g. "ERROR AUTH_FAILED").
struct JoinResult {
    bool ok;
    char color;       // 'w' | 'b', meaningful only when ok == true
    bool hasOpponent; // true when the other color slot was already filled
                      // before this join (i.e. this is the 2nd successful
                      // join) - tells the caller whether an already-connected
                      // opponent needs notifying
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
// Logger itself is owned externally (WebSocketServer, later shared across
// every session once SessionManager exists in Task D1) - not per-session,
// so multiple concurrent sessions can log to one shared file.
class GameSession {
    GameEngine engine_;
    Logger* logger_ = nullptr;
    // Task C3: not optional in real production use (WebSocketServer always
    // attaches a real one) - but kept as an optional-attach pointer, same
    // pattern as logger_, so every pre-existing GameSession()-default-
    // constructing test that never calls handleJoin() at all (the
    // handleCommand/logger tests) keeps working unchanged. Unlike logger_'s
    // silent-no-op-when-absent default, handleJoin() fails *closed*
    // (ERROR AUTH_NOT_CONFIGURED) when authService_ is null - "no auth
    // configured" must never silently mean "let everyone in".
    AuthService* authService_ = nullptr;
    bool whiteJoined_ = false;
    bool blackJoined_ = false;
    std::string whiteUsername_;
    std::string blackUsername_;

    void logEvent(const std::string& message);

public:
    GameSession();

    GameEngine& engine() { return engine_; }
    void attachLogger(Logger& logger) { logger_ = &logger; }
    void attachAuthService(AuthService& auth) { authService_ = &auth; }

    CommandResult handleCommand(const ParsedCommand& cmd);
    // Authenticates via AuthService (auto-register on a never-seen-before
    // username, per C4) before any color assignment happens - see
    // AuthService::authenticate() for the accept/reject decision itself.
    JoinResult handleJoin(const std::string& username, const std::string& password);
};
#endif
