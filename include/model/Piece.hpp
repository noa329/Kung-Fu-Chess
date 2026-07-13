#ifndef PIECE_H
#define PIECE_H
#include <string>
#include "Position.hpp"
#include "PieceKind.hpp"

// שכבת Model: זהות כלי בלבד. אין כאן שום כלל תנועה - זה תפקידה
// של שכבת MovementRules (ראו MovementRules.hpp).
class Piece {
protected:
    char color; // 'w' or 'b'
public:
    Piece(char c) : color(c) {}
    virtual ~Piece() = default;
    char getColor() const { return color; }
    virtual PieceKind getKind() const = 0;
    virtual std::string toString() const = 0;

    bool isKing() const { return getKind() == PieceKind::King; }
    bool isPawn() const { return getKind() == PieceKind::Pawn; }
};
#endif
