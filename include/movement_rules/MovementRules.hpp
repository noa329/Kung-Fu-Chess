#ifndef MOVEMENT_RULES_H
#define MOVEMENT_RULES_H
#include "Position.hpp"
#include "PieceKind.hpp"

namespace MovementRules {
    bool isValidShape(PieceKind kind, char color, const Position& from, const Position& to, int boardRows);

    bool isValidCapture(PieceKind kind, char color, const Position& from, const Position& to, int boardRows);

    bool isSliding(PieceKind kind);
}
#endif
