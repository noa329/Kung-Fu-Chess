#include "doctest.h"
#include "KingMovement.hpp"
#include "RookMovement.hpp"
#include "BishopMovement.hpp"
#include "QueenMovement.hpp"
#include "KnightMovement.hpp"
#include "PawnMovement.hpp"
#include "MovementStrategyFactory.hpp"

// Strategy pattern: כל כלי מקבל מחלקה נפרדת שמממשת IMovementStrategy.
// כל מחלקה כאן היא היחידה שיודעת את הגיאומטריה של הכלי שלה - הוספת
// כלי חדש בעתיד לא תדרוש לגעת באף אחת מהמחלקות הקיימות.

TEST_CASE("KingMovement allows one step, rejects two") {
    KingMovement king;
    CHECK(king.isValidShape('w', {2,2}, {3,3}, 8) == true);
    CHECK(king.isValidShape('w', {2,2}, {4,2}, 8) == false);
    CHECK(king.isSliding() == false);
}

TEST_CASE("RookMovement allows straight lines only") {
    RookMovement rook;
    CHECK(rook.isValidShape('w', {0,0}, {0,5}, 8) == true);
    CHECK(rook.isValidShape('w', {0,0}, {5,5}, 8) == false);
    CHECK(rook.isSliding() == true);
}

TEST_CASE("BishopMovement allows diagonals only") {
    BishopMovement bishop;
    CHECK(bishop.isValidShape('w', {0,0}, {3,3}, 8) == true);
    CHECK(bishop.isValidShape('w', {0,0}, {0,3}, 8) == false);
    CHECK(bishop.isSliding() == true);
}

TEST_CASE("QueenMovement allows straight or diagonal") {
    QueenMovement queen;
    CHECK(queen.isValidShape('w', {0,0}, {0,5}, 8) == true);
    CHECK(queen.isValidShape('w', {0,0}, {3,3}, 8) == true);
    CHECK(queen.isValidShape('w', {0,0}, {1,2}, 8) == false);
}

TEST_CASE("KnightMovement allows only L shapes") {
    KnightMovement knight;
    CHECK(knight.isValidShape('b', {4,4}, {6,5}, 8) == true);
    CHECK(knight.isValidShape('b', {4,4}, {5,5}, 8) == false);
    CHECK(knight.isSliding() == false);
}

TEST_CASE("PawnMovement has a distinct capture rule from its move rule") {
    PawnMovement pawn;
    CHECK(pawn.isValidShape('w', {5,3}, {4,3}, 8) == true);   // צעד ישר
    CHECK(pawn.isValidCapture('w', {5,3}, {4,3}, 8) == false); // ישר - לא חוקי לאכילה
    CHECK(pawn.isValidCapture('w', {5,3}, {4,2}, 8) == true);  // אלכסון - חוקי לאכילה
    CHECK(pawn.isSliding() == true);
}

TEST_CASE("MovementStrategyFactory returns the matching strategy per kind") {
    CHECK(getMovementStrategy(PieceKind::King).isValidShape('w', {2,2}, {3,3}, 8) == true);
    CHECK(getMovementStrategy(PieceKind::Rook).isSliding() == true);
    CHECK(getMovementStrategy(PieceKind::Knight).isSliding() == false);
}
