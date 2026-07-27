#include "NetworkClickHandler.hpp"

namespace {

bool inBounds(const GameSnapshot& snap, const Position& pos) {
    return pos.row >= 0 && pos.row < static_cast<int>(snap.boardTokens.size()) && pos.col >= 0
           && pos.col < static_cast<int>(snap.boardTokens[pos.row].size());
}

std::string tokenAt(const GameSnapshot& snap, const Position& pos) {
    if (!inBounds(snap, pos)) return ".";
    return snap.boardTokens[pos.row][pos.col];
}

bool isOwnPiece(const std::string& token, char myColor) {
    return token.size() == 2 && token[0] == myColor;
}

// Fixed 8x8, same documented assumption GameCommandParser.hpp's own wire
// grammar makes: the wire protocol only ever talks to a real 8x8 game,
// unlike GameEngine's own tests which exercise arbitrary board sizes.
// Mirrors game_engine/GameEngine.cpp's own private squareName() formula
// exactly (rowCount fixed at 8 here instead of taken as a parameter).
std::string squareName(const Position& pos) {
    return std::string(1, char('a' + pos.col)) + std::to_string(8 - pos.row);
}

} // namespace

std::optional<std::string> NetworkClickHandler::handleClick(const GameSnapshot& snapshot, const Position& pos) {
    std::string token = tokenAt(snapshot, pos);

    if (!hasPending_) {
        if (isOwnPiece(token, myColor_)) {
            pendingFrom_ = pos;
            hasPending_ = true;
        }
        return std::nullopt;
    }

    std::string fromToken = tokenAt(snapshot, pendingFrom_);
    Position from = pendingFrom_;
    hasPending_ = false;

    if (isOwnPiece(token, myColor_)) {
        // Re-select, mirrors GameEngine::select() switching `selected`
        // instead of attempting an illegal same-color "capture".
        pendingFrom_ = pos;
        hasPending_ = true;
        return std::nullopt;
    }

    if (!isOwnPiece(fromToken, myColor_)) {
        // The pending piece moved/was captured since the first click (a
        // real scenario - the board isn't turn-locked) - nothing sane to
        // build from stale data.
        return std::nullopt;
    }

    char colorChar = (myColor_ == 'w') ? 'W' : 'B';
    std::string command;
    command += colorChar;
    command += fromToken[1];
    command += squareName(from);
    command += squareName(pos);
    return command;
}

std::optional<std::string> NetworkClickHandler::handleJump(const GameSnapshot& snapshot, const Position& pos) const {
    std::string token = tokenAt(snapshot, pos);
    if (!isOwnPiece(token, myColor_)) return std::nullopt;

    char colorChar = (myColor_ == 'w') ? 'W' : 'B';
    std::string command = "J";
    command += colorChar;
    command += token[1];
    command += squareName(pos);
    return command;
}
