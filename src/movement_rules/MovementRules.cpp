#include "movement_rules/MovementRules.hpp"
#include "movement_rules/MovementStrategyFactory.hpp"

namespace MovementRules {

bool isValidShape(PieceKind kind, char color, const Position& from, const Position& to, int boardRows) {
    return getMovementStrategy(kind).isValidShape(color, from, to, boardRows);
}

bool isValidCapture(PieceKind kind, char color, const Position& from, const Position& to, int boardRows) {
    return getMovementStrategy(kind).isValidCapture(color, from, to, boardRows);
}

bool isSliding(PieceKind kind) {
    return getMovementStrategy(kind).isSliding();
}

}
