#include "rule_engine/RuleEngine.hpp"
#include "movement_rules/MovementRules.hpp"

bool RuleEngine::isLegal(const std::shared_ptr<Piece>& piece, const Position& from,
                          const Position& to, bool isCapture) const {
    int rows = board.getRowCount();
    PieceKind kind = piece->getKind();
    char color = piece->getColor();

    bool validShape = isCapture ? MovementRules::isValidCapture(kind, color, from, to, rows)
                                 : MovementRules::isValidShape(kind, color, from, to, rows);
    if (!validShape) return false;

    if (MovementRules::isSliding(kind) && !board.isPathClear(from, to)) return false;

    return true;
}
