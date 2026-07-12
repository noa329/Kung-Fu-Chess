#ifndef KING_MOVEMENT_H
#define KING_MOVEMENT_H
#include "IMovementStrategy.hpp"

class KingMovement : public IMovementStrategy {
public:
    bool isValidShape(char color, const Position& from, const Position& to, int boardRows) const override;
    bool isSliding() const override { return false; }
};
#endif
