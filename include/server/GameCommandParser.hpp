#ifndef SERVER_GAME_COMMAND_PARSER_H
#define SERVER_GAME_COMMAND_PARSER_H
#include "Position.hpp"
#include <string>

// Wire command grammar (design decision, Task A2 - the deck only gave one
// example, "WQe2e5", with no jump/resign/ready grammar specified):
//
//   Move: <Color><Piece><From><To>   e.g. "WQe2e5" = White Queen, e2 -> e5
//   Jump: J<Color><Piece><Square>    e.g. "JWPe2"  = Jump, White Pawn at e2
//
// - Color is exactly 'W' or 'B' (uppercase; normalized to Piece::getColor()'s
//   'w'/'b' convention in the parsed result).
// - Piece is exactly one of P/N/B/R/Q/K (uppercase; matches the second
//   character of Piece::toString(), e.g. "wQ").
// - Squares are algebraic ("e2": file a-h, rank 1-8), always relative to a
//   standard 8x8 board - unlike GameEngine's own tests (which exercise
//   arbitrary board sizes internally via squareName(pos, rowCount)), the
//   wire protocol only ever talks to a real 8x8 game, so the file/rank ->
//   Position conversion doesn't need a board-size parameter.
// - A leading "J" is what distinguishes a jump from a move, since jump
//   maps to a different GameEngine call (jump(pos) vs select(from) then
//   select(to)) and only ever needs one square, not two.
// - Casing is strict (no lowercase color/piece, no lowercase "j") - a
//   deliberate simplicity choice, not a hard protocol requirement; revisit
//   if case-insensitivity turns out to matter once a real client exists.
//
// This parser only checks the command's own shape. Whether the named
// piece/color actually matches what's on the board at `from` is
// GameSession's job (Task A3), since that requires a Board this parser
// deliberately doesn't depend on.

struct ParsedCommand {
    bool isJump;
    char color;        // 'w' | 'b'
    char pieceLetter;  // 'P' | 'N' | 'B' | 'R' | 'Q' | 'K'
    Position from;
    Position to;       // meaningful only when isJump == false
};

struct ParseResult {
    bool ok;
    ParsedCommand command; // meaningful only when ok == true
    std::string error;     // set only when ok == false, e.g. "ERROR INVALID_SQUARE"
};

namespace GameCommandParser {
    ParseResult parse(const std::string& raw);
}
#endif
