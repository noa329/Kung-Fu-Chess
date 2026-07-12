#include "movement_rules/RookMovement.hpp"

bool RookMovement::isValidShape(char, const Position& from, const Position& to, int) const {
    if (from.row == to.row && from.col == to.col) return false;
    return from.row == to.row || from.col == to.col;
}
