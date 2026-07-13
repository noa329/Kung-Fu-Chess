#include "KnightMovement.hpp"
#include <cstdlib>

bool KnightMovement::isValidShape(char, const Position& from, const Position& to, int) const {
    int dr = std::abs(to.row - from.row);
    int dc = std::abs(to.col - from.col);
    return (dr == 1 && dc == 2) || (dr == 2 && dc == 1);
}
