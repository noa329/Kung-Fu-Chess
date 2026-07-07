#ifndef PIECE_H
#define PIECE_H
#include <string>
#include "Position.h"

class Piece {
protected:
    char color; // 'w' or 'b'
public:
    Piece(char c) : color(c) {}
    virtual ~Piece() = default;
    char getColor() const { return color; }
    virtual bool isValidShape(const Position& from, const Position& to) const = 0;
    virtual std::string toString() const = 0;
};
#endif