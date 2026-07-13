#include "doctest.h"
#include "RuleEngine.hpp"
#include "Board.hpp"
#include "Pieces.hpp"

// שכבת RuleEngine: מאמתת חוקיות מהלך מבוקש מול מצב הלוח הנוכחי - קריאה בלבד.
// לא יודעת כלום על תנועות ממתינות/זמן (זה RealTimeArbiter) ולא משנה את הלוח.

TEST_CASE("RuleEngine allows a clear straight rook move") {
    Board board;
    board.setGrid({
        {"wR", ".", "."},
        {".", ".", "."},
        {".", ".", "."}
    });
    RuleEngine engine(board);
    auto rook = board.getCell(0, 0);
    CHECK(engine.isLegal(rook, {0,0}, {0,2}, false) == true);
}

TEST_CASE("RuleEngine blocks a rook move when the path is not clear") {
    Board board;
    board.setGrid({
        {"wR", "wP", "."},
        {".", ".", "."},
        {".", ".", "."}
    });
    RuleEngine engine(board);
    auto rook = board.getCell(0, 0);
    CHECK(engine.isLegal(rook, {0,0}, {0,2}, false) == false);
}

TEST_CASE("RuleEngine rejects a geometrically invalid shape") {
    Board board;
    board.setGrid({
        {"wB", ".", "."},
        {".", ".", "."},
        {".", ".", "."}
    });
    RuleEngine engine(board);
    auto bishop = board.getCell(0, 0);
    CHECK(engine.isLegal(bishop, {0,0}, {0,2}, false) == false); // רץ לא זז ישר
}

TEST_CASE("RuleEngine lets a knight jump over an occupied path") {
    Board board;
    board.setGrid({
        {"bN", "wP", "."},
        {".", ".", "."},
        {".", ".", "."}
    });
    RuleEngine engine(board);
    auto knight = board.getCell(0, 0);
    CHECK(engine.isLegal(knight, {0,0}, {1,2}, false) == true);
}

TEST_CASE("RuleEngine validates pawn capture shape separately from move shape") {
    Board board;
    board.setGrid({
        {".", ".", "."},
        {".", "wP", "."},
        {"bP", ".", "."}
    });
    RuleEngine engine(board);
    auto pawn = board.getCell(1, 1); // חייל לבן זז כלפי row קטן יותר
    CHECK(engine.isLegal(pawn, {1,1}, {2,0}, /*isCapture=*/true) == false); // כיוון הפוך - לא חוקי
    CHECK(engine.isLegal(pawn, {1,1}, {0,0}, /*isCapture=*/true) == true);  // אלכסון קדימה - חוקי
    CHECK(engine.isLegal(pawn, {1,1}, {0,1}, /*isCapture=*/true) == false); // ישר - לא חוקי לאכילה
}
