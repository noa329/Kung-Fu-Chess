#include "PawnMovement.hpp"
#include <cstdlib>

bool PawnMovement::isValidShape(char color, const Position& from, const Position& to, int boardRows) const {
    int dr = to.row - from.row;
    int dc = to.col - from.col;
    if (dc != 0) return false;
    int direction = (color == 'w') ? -1 : 1;
    if (dr == direction) return true; // צעד יחיד

    int startRow = (color == 'w') ? boardRows - 1 : 0;
    if (dr == 2 * direction && from.row == startRow) return true; // צעד כפול, רק משורת הבית
    return false;
}

bool PawnMovement::isValidCapture(char color, const Position& from, const Position& to, int) const {
    int dr = to.row - from.row;
    int dc = std::abs(to.col - from.col);
    int direction = (color == 'w') ? -1 : 1;
    return dc == 1 && dr == direction;
}
