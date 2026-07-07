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
    virtual bool isValidCapture(const Position& from, const Position& to) const {
        return isValidShape(from, to); // ברירת מחדל: אכילה נראית כמו הזזה רגילה
    }
    virtual bool isSliding() const { return false; }
    virtual std::string toString() const = 0;
};
#endif