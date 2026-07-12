#include "doctest.h"
#include "../include/MovementRules.hpp"

// שכבת Movement Rules: גיאומטריית תנועה טהורה לכל סוג כלי.
// אין כאן שום תלות ב-Piece, ב-Board, בזמן או ברינדור - רק (kind, color, from, to, boardRows) -> bool.

TEST_CASE("King moves one step in any direction") {
    CHECK(MovementRules::isValidShape(PieceKind::King, 'w', {2,2}, {2,2}, 8) == false);
    CHECK(MovementRules::isValidShape(PieceKind::King, 'w', {2,2}, {3,3}, 8) == true);
    CHECK(MovementRules::isValidShape(PieceKind::King, 'w', {2,2}, {3,2}, 8) == true);
    CHECK(MovementRules::isValidShape(PieceKind::King, 'w', {2,2}, {4,2}, 8) == false);
}

TEST_CASE("Rook moves in straight lines only") {
    CHECK(MovementRules::isValidShape(PieceKind::Rook, 'w', {0,0}, {0,5}, 8) == true);
    CHECK(MovementRules::isValidShape(PieceKind::Rook, 'w', {0,0}, {5,0}, 8) == true);
    CHECK(MovementRules::isValidShape(PieceKind::Rook, 'w', {0,0}, {5,5}, 8) == false);
}

TEST_CASE("Bishop moves diagonally only") {
    CHECK(MovementRules::isValidShape(PieceKind::Bishop, 'w', {0,0}, {3,3}, 8) == true);
    CHECK(MovementRules::isValidShape(PieceKind::Bishop, 'w', {0,0}, {0,3}, 8) == false);
}

TEST_CASE("Queen moves straight or diagonal") {
    CHECK(MovementRules::isValidShape(PieceKind::Queen, 'w', {0,0}, {0,5}, 8) == true);
    CHECK(MovementRules::isValidShape(PieceKind::Queen, 'w', {0,0}, {3,3}, 8) == true);
    CHECK(MovementRules::isValidShape(PieceKind::Queen, 'w', {0,0}, {1,2}, 8) == false);
}

TEST_CASE("Knight moves in L shape") {
    CHECK(MovementRules::isValidShape(PieceKind::Knight, 'b', {4,4}, {6,5}, 8) == true);
    CHECK(MovementRules::isValidShape(PieceKind::Knight, 'b', {4,4}, {5,5}, 8) == false);
}

TEST_CASE("Pawn double move only from starting row") {
    CHECK(MovementRules::isValidShape(PieceKind::Pawn, 'w', {7,3}, {5,3}, 8) == true);
    CHECK(MovementRules::isValidShape(PieceKind::Pawn, 'w', {5,3}, {3,3}, 8) == false);
    CHECK(MovementRules::isValidShape(PieceKind::Pawn, 'w', {5,3}, {4,3}, 8) == true);
}

TEST_CASE("Pawn captures diagonally only") {
    CHECK(MovementRules::isValidCapture(PieceKind::Pawn, 'b', {2,3}, {3,4}, 8) == true);
    CHECK(MovementRules::isValidCapture(PieceKind::Pawn, 'b', {2,3}, {3,3}, 8) == false);
}

TEST_CASE("isSliding reflects which kinds require a clear path") {
    CHECK(MovementRules::isSliding(PieceKind::Rook) == true);
    CHECK(MovementRules::isSliding(PieceKind::Bishop) == true);
    CHECK(MovementRules::isSliding(PieceKind::Queen) == true);
    CHECK(MovementRules::isSliding(PieceKind::Pawn) == true);
    CHECK(MovementRules::isSliding(PieceKind::King) == false);
    CHECK(MovementRules::isSliding(PieceKind::Knight) == false);
}
