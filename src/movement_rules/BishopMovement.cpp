#include "movement_rules/BishopMovement.hpp"
#include <cstdlib>

bool BishopMovement::isValidShape(char, const Position& from, const Position& to, int) const {
    int dr = std::abs(to.row - from.row);
    int dc = std::abs(to.col - from.col);
    return dr == dc && dr > 0;
}
