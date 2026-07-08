#include "doctest.h"
#include "../include/Pieces.hpp"

TEST_CASE("King moves one step in any direction") {
    King king('w');
    CHECK(king.isValidShape({2,2}, {2,2}, 8) == false); // אותו מקום - לא חוקי
    CHECK(king.isValidShape({2,2}, {3,3}, 8) == true);  // אלכסון צעד אחד
    CHECK(king.isValidShape({2,2}, {3,2}, 8) == true);  // אנכי צעד אחד
    CHECK(king.isValidShape({2,2}, {4,2}, 8) == false); // צעדיים - לא חוקי
}

TEST_CASE("Rook moves in straight lines only") {
    Rook rook('w');
    CHECK(rook.isValidShape({0,0}, {0,5}, 8) == true);  // שורה זהה
    CHECK(rook.isValidShape({0,0}, {5,0}, 8) == true);  // עמודה זהה
    CHECK(rook.isValidShape({0,0}, {5,5}, 8) == false); // אלכסון - לא חוקי
}

TEST_CASE("Knight moves in L shape") {
    Knight knight('b');
    CHECK(knight.isValidShape({4,4}, {6,5}, 8) == true);
    CHECK(knight.isValidShape({4,4}, {5,5}, 8) == false);
}

TEST_CASE("Pawn double move only from starting row") {
    Pawn whitePawn('w');
    // בלוח בגודל 8, שורת ההתחלה של לבן היא row=7
    CHECK(whitePawn.isValidShape({7,3}, {5,3}, 8) == true);  // צעד כפול משורת הבית
    CHECK(whitePawn.isValidShape({5,3}, {3,3}, 8) == false); // צעד כפול משורה אחרת - לא חוקי
    CHECK(whitePawn.isValidShape({5,3}, {4,3}, 8) == true);  // צעד יחיד - תמיד חוקי
}

TEST_CASE("Pawn captures diagonally only") {
    Pawn blackPawn('b');
    CHECK(blackPawn.isValidCapture({2,3}, {3,4}, 8) == true);
    CHECK(blackPawn.isValidCapture({2,3}, {3,3}, 8) == false); // ישר - לא חוקי לאכילה
}