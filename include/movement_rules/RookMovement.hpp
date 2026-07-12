#ifndef ROOK_MOVEMENT_H
#define ROOK_MOVEMENT_H
#include "IMovementStrategy.hpp"

class RookMovement : public IMovementStrategy {
public:
    bool isValidShape(char color, const Position& from, const Position& to, int boardRows) const override;
    bool isSliding() const override { return true; }
};
#endif
