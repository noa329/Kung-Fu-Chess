#include "GameCommandParser.hpp"

namespace {

bool isValidPieceLetter(char c) {
    return c == 'P' || c == 'N' || c == 'B' || c == 'R' || c == 'Q' || c == 'K';
}

// "e2" -> Position, assuming a standard 8x8 board. Inverse of GameEngine's
// private squareName(), but fixed at rowCount=8 since wire commands only
// ever address a real game (see the grammar comment in the header).
bool parseSquare(char fileChar, char rankChar, Position& out) {
    if (fileChar < 'a' || fileChar > 'h') return false;
    if (rankChar < '1' || rankChar > '8') return false;
    out.col = fileChar - 'a';
    out.row = 8 - (rankChar - '0');
    return true;
}

ParseResult fail(const std::string& error) {
    ParseResult result;
    result.ok = false;
    result.error = error;
    return result;
}

} // namespace

namespace GameCommandParser {

ParseResult parse(const std::string& raw) {
    if (raw.empty()) return fail("ERROR MALFORMED_COMMAND");

    bool isJump = (raw[0] == 'J');
    size_t expectedLength = isJump ? 5 : 6;
    if (raw.size() != expectedLength) return fail("ERROR MALFORMED_COMMAND");

    size_t colorIdx = isJump ? 1 : 0;
    size_t pieceIdx = isJump ? 2 : 1;
    size_t fromFileIdx = isJump ? 3 : 2;

    char colorChar = raw[colorIdx];
    if (colorChar != 'W' && colorChar != 'B') return fail("ERROR INVALID_COLOR");

    char pieceChar = raw[pieceIdx];
    if (!isValidPieceLetter(pieceChar)) return fail("ERROR INVALID_PIECE");

    Position from;
    if (!parseSquare(raw[fromFileIdx], raw[fromFileIdx + 1], from)) {
        return fail("ERROR INVALID_SQUARE");
    }

    ParsedCommand cmd;
    cmd.isJump = isJump;
    cmd.color = (colorChar == 'W') ? 'w' : 'b';
    cmd.pieceLetter = pieceChar;
    cmd.from = from;

    if (!isJump) {
        Position to;
        if (!parseSquare(raw[4], raw[5], to)) return fail("ERROR INVALID_SQUARE");
        cmd.to = to;
    }

    ParseResult result;
    result.ok = true;
    result.command = cmd;
    return result;
}

} // namespace GameCommandParser
