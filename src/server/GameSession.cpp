#include "GameSession.hpp"

namespace {
CommandResult fail(const std::string& error) {
    return {false, error};
}
} // namespace

CommandResult GameSession::handleCommand(const ParsedCommand& cmd) {
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
