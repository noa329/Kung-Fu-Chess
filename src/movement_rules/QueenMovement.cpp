#include "QueenMovement.hpp"
#include <cstdlib>

bool QueenMovement::isValidShape(char, const Position& from, const Position& to, int) const {
    int dr = std::abs(to.row - from.row);
    int dc = std::abs(to.col - from.col);
    if (dr == 0 && dc == 0) return false;
    return (from.row == to.row || from.col == to.col) || (dr == dc);
}
