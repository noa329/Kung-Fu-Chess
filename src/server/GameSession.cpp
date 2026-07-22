#include "GameSession.hpp"

namespace {
CommandResult fail(const std::string& error) {
    return {false, error};
}
} // namespace

void GameSession::logEvent(const std::string& message) {
    if (logger_) logger_->log(message);
}

GameSession::GameSession() {
    engine_.events().onMoveLogged.subscribe([this](const MoveLoggedEvent& e) {
        logEvent("move color=" + std::string(1, e.color) + " notation=" + e.notation
                  + " atMs=" + std::to_string(e.atMs));
    });
    engine_.events().onScoreUpdated.subscribe([this](const ScoreUpdatedEvent& e) {
        logEvent("score color=" + std::string(1, e.color) + " newScore=" + std::to_string(e.newScore)
                  + " delta=" + std::to_string(e.delta));
    });
    engine_.events().onGameLifecycle.subscribe([this](const GameLifecycleEvent& e) {
        std::string message = "lifecycle phase=" + e.phase;
        if (e.phase == "end") message += " result=" + e.result;
        logEvent(message);
    });
    engine_.events().onSound.subscribe([this](const SoundEvent& e) {
        logEvent("sound name=" + e.name);
    });
}

CommandResult GameSession::handleCommand(const std::string& username, const ParsedCommand& cmd) {
    char senderColor = colorOf(username);
    if (senderColor == '\0') return fail("ERROR NOT_A_PLAYER");
    if (senderColor != cmd.color) return fail("ERROR NOT_YOUR_COLOR");

    GameSnapshot snap = engine_.snapshot();
    int row = cmd.from.row;
    int col = cmd.from.col;

    bool inBounds = row >= 0 && row < static_cast<int>(snap.boardTokens.size())
                     && col >= 0 && col < static_cast<int>(snap.boardTokens[row].size());
    if (!inBounds) return fail("ERROR NO_PIECE_AT_SQUARE");

    const std::string& token = snap.boardTokens[row][col];
    if (token == ".") return fail("ERROR NO_PIECE_AT_SQUARE");

    char boardColor = token[0];
    char boardPieceLetter = token[1];
    if (boardColor != cmd.color) return fail("ERROR COLOR_MISMATCH");
    if (boardPieceLetter != cmd.pieceLetter) return fail("ERROR PIECE_MISMATCH");

    if (cmd.isJump) {
        engine_.jump(cmd.from);
    } else {
        engine_.select(cmd.from);
        engine_.select(cmd.to);
    }
    return {true, ""};
}

JoinResult GameSession::join(const std::string& username) {
    if (!whiteJoined_) {
        whiteJoined_ = true;
        whiteUsername_ = username;
        return {true, 'w', false, ""};
    }
    if (!blackJoined_) {
        blackJoined_ = true;
        blackUsername_ = username;
        return {true, 'b', true, ""};
    }
    spectatorUsernames_.push_back(username);
    return {true, 's', true, ""};
}

char GameSession::colorOf(const std::string& username) const {
    if (whiteJoined_ && whiteUsername_ == username) return 'w';
    if (blackJoined_ && blackUsername_ == username) return 'b';
    return '\0';
}

bool GameSession::isSpectator(const std::string& username) const {
    for (const auto& name : spectatorUsernames_) {
        if (name == username) return true;
    }
    return false;
}

std::string GameSession::usernameFor(char color) const {
    if (color == 'w') return whiteUsername_;
    if (color == 'b') return blackUsername_;
    return "";
}

void GameSession::markDisconnected(char color) {
    if (color == 'w') whiteConnected_ = false;
    else if (color == 'b') blackConnected_ = false;
}

void GameSession::markReconnected(char color) {
    if (color == 'w') whiteConnected_ = true;
    else if (color == 'b') blackConnected_ = true;
}

bool GameSession::isConnected(char color) const {
    if (color == 'w') return whiteConnected_;
    if (color == 'b') return blackConnected_;
    return true;
}
