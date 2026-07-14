#ifndef PAWN_MOVEMENT_H
#define PAWN_MOVEMENT_H
#include "IMovementStrategy.hpp"

class PawnMovement : public IMovementStrategy {
public:
    bool isValidShape(char color, const Position& from, const Position& to, int boardRows) const override;
    bool isValidCapture(char color, const Position& from, const Position& to, int boardRows) const override;
    bool isSliding() const override { return true; } 
};
#endif
