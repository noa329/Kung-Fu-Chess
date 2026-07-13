#include "KingMovement.hpp"
#include <cstdlib>

bool KingMovement::isValidShape(char, const Position& from, const Position& to, int) const {
    int dr = std::abs(to.row - from.row);
    int dc = std::abs(to.col - from.col);
    return dr <= 1 && dc <= 1 && (dr + dc > 0);
}
