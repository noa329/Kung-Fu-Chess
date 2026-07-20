#ifndef SERVER_GAME_SESSION_H
#define SERVER_GAME_SESSION_H
#include "GameEngine.hpp"
#include "GameCommandParser.hpp"
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
class GameSession {
    GameEngine engine_;

public:
    GameEngine& engine() { return engine_; }

    CommandResult handleCommand(const ParsedCommand& cmd);
};
#endif
