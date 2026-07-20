#include "doctest.h"
#include "GameCommandParser.hpp"

// server layer: GameCommandParser turns the wire command string
// ("WQe2e5" for moves, "JWPe2" for jumps - see the design-decision
// comment in GameCommandParser.hpp) into a ParsedCommand. Pure parsing
// only - no board, no GameEngine, no networking. Validating the parsed
// piece letter against what's actually on the board is GameSession's job
// (Task A3), not this parser's.

TEST_CASE("a valid move command parses color, piece, and both squares") {
    auto result = GameCommandParser::parse("WQe2e5");
    REQUIRE(result.ok);
    CHECK(result.command.isJump == false);
    CHECK(result.command.color == 'w');
    CHECK(result.command.pieceLetter == 'Q');
    CHECK(result.command.from == Position{6, 4}); // e2: file e -> col 4, rank 2 -> row 8-2=6
    CHECK(result.command.to == Position{3, 4});   // e5: rank 5 -> row 8-5=3
}

TEST_CASE("a valid move command for black normalizes color to lowercase") {
    auto result = GameCommandParser::parse("BKe7e8");
    REQUIRE(result.ok);
    CHECK(result.command.color == 'b');
    CHECK(result.command.pieceLetter == 'K');
    CHECK(result.command.from == Position{1, 4}); // e7
    CHECK(result.command.to == Position{0, 4});   // e8
}

TEST_CASE("a valid jump command parses color, piece, and the single square") {
    auto result = GameCommandParser::parse("JWPe2");
    REQUIRE(result.ok);
    CHECK(result.command.isJump == true);
    CHECK(result.command.color == 'w');
    CHECK(result.command.pieceLetter == 'P');
    CHECK(result.command.from == Position{6, 4}); // e2
}

TEST_CASE("a jump command for black normalizes color to lowercase") {
    auto result = GameCommandParser::parse("JBNa1");
    REQUIRE(result.ok);
    CHECK(result.command.isJump == true);
    CHECK(result.command.color == 'b');
    CHECK(result.command.pieceLetter == 'N');
    CHECK(result.command.from == Position{7, 0}); // a1
}

TEST_CASE("an empty command is rejected as malformed") {
    auto result = GameCommandParser::parse("");
    CHECK(result.ok == false);
    CHECK(result.error == "ERROR MALFORMED_COMMAND");
}

TEST_CASE("a too-short garbage string is rejected as malformed") {
    auto result = GameCommandParser::parse("abc");
    CHECK(result.ok == false);
    CHECK(result.error == "ERROR MALFORMED_COMMAND");
}

TEST_CASE("a move command with the wrong length is rejected as malformed") {
    CHECK(GameCommandParser::parse("WQe2e").ok == false);   // one char short
    CHECK(GameCommandParser::parse("WQe2e55").ok == false); // one char long
}

TEST_CASE("a jump command with the wrong length is rejected as malformed") {
    CHECK(GameCommandParser::parse("JWP").ok == false);  // too short
    CHECK(GameCommandParser::parse("JWPe22").ok == false); // too long
}

TEST_CASE("an invalid color character is rejected") {
    auto result = GameCommandParser::parse("XQe2e5");
    CHECK(result.ok == false);
    CHECK(result.error == "ERROR INVALID_COLOR");
}

TEST_CASE("lowercase color is rejected - the wire format is strictly uppercase") {
    auto result = GameCommandParser::parse("wqe2e5");
    CHECK(result.ok == false);
    CHECK(result.error == "ERROR INVALID_COLOR");
}

TEST_CASE("an invalid piece letter is rejected") {
    auto result = GameCommandParser::parse("WXe2e5");
    CHECK(result.ok == false);
    CHECK(result.error == "ERROR INVALID_PIECE");
}

TEST_CASE("an out-of-range square is rejected") {
    auto result = GameCommandParser::parse("WQz9z9");
    CHECK(result.ok == false);
    CHECK(result.error == "ERROR INVALID_SQUARE");
}

TEST_CASE("an out-of-range destination square is rejected even when the origin is valid") {
    auto result = GameCommandParser::parse("WQe2z9");
    CHECK(result.ok == false);
    CHECK(result.error == "ERROR INVALID_SQUARE");
}

TEST_CASE("an out-of-range square in a jump command is rejected") {
    auto result = GameCommandParser::parse("JWPz9");
    CHECK(result.ok == false);
    CHECK(result.error == "ERROR INVALID_SQUARE");
}
