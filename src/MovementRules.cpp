#include "MovementRules.hpp"
#include <cstdlib>

namespace {
    bool kingShape(const Position& from, const Position& to) {
        int dr = std::abs(to.row - from.row);
        int dc = std::abs(to.col - from.col);
        return dr <= 1 && dc <= 1 && (dr + dc > 0);
    }

    bool rookShape(const Position& from, const Position& to) {
        if (from.row == to.row && from.col == to.col) return false;
        return from.row == to.row || from.col == to.col;
    }

    bool bishopShape(const Position& from, const Position& to) {
        int dr = std::abs(to.row - from.row);
        int dc = std::abs(to.col - from.col);
        return dr == dc && dr > 0;
    }

    bool queenShape(const Position& from, const Position& to) {
        int dr = std::abs(to.row - from.row);
        int dc = std::abs(to.col - from.col);
        if (dr == 0 && dc == 0) return false;
        return (from.row == to.row || from.col == to.col) || (dr == dc);
    }

    bool knightShape(const Position& from, const Position& to) {
        int dr = std::abs(to.row - from.row);
        int dc = std::abs(to.col - from.col);
        return (dr == 1 && dc == 2) || (dr == 2 && dc == 1);
    }

    bool pawnShape(char color, const Position& from, const Position& to, int boardRows) {
        int dr = to.row - from.row;
        int dc = to.col - from.col;
        if (dc != 0) return false;
        int direction = (color == 'w') ? -1 : 1;
        if (dr == direction) return true; // צעד יחיד

        int startRow = (color == 'w') ? boardRows - 1 : 0; // שורת הקצה של הצבע
        if (dr == 2 * direction && from.row == startRow) return true; // צעד כפול, רק משורת הבית
        return false;
    }

    bool pawnCapture(char color, const Position& from, const Position& to) {
        int dr = to.row - from.row;
        int dc = std::abs(to.col - from.col);
        int direction = (color == 'w') ? -1 : 1;
        return dc == 1 && dr == direction;
    }
}

namespace MovementRules {

bool isValidShape(PieceKind kind, char color, const Position& from, const Position& to, int boardRows) {
    switch (kind) {
        case PieceKind::King:   return kingShape(from, to);
        case PieceKind::Rook:   return rookShape(from, to);
        case PieceKind::Bishop: return bishopShape(from, to);
        case PieceKind::Queen:  return queenShape(from, to);
        case PieceKind::Knight: return knightShape(from, to);
        case PieceKind::Pawn:   return pawnShape(color, from, to, boardRows);
    }
    return false;
}

bool isValidCapture(PieceKind kind, char color, const Position& from, const Position& to, int boardRows) {
    if (kind == PieceKind::Pawn) return pawnCapture(color, from, to);
    return isValidShape(kind, color, from, to, boardRows);
}

bool isSliding(PieceKind kind) {
    switch (kind) {
        case PieceKind::Rook:
        case PieceKind::Bishop:
        case PieceKind::Queen:
        case PieceKind::Pawn:
            return true;
        default:
            return false;
    }
}

}
