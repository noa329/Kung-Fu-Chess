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

    void logEvent(const std::string& message);

public:
    GameSession();

    GameEngine& engine() { return engine_; }
    void attachLogger(Logger& logger) { logger_ = &logger; }

    CommandResult handleCommand(const ParsedCommand& cmd);
};
#endif
