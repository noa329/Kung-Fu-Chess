#ifndef PIECES_H
#define PIECES_H
#include <cstdlib>
#include "Piece.h"

class King : public Piece {
public:
    King(char c) : Piece(c) {}
    bool isValidShape(const Position& from, const Position& to) const override {
        int dr = std::abs(to.row - from.row);
        int dc = std::abs(to.col - from.col);
        return dr <= 1 && dc <= 1 && (dr + dc > 0);
    }
    bool isKing() const override { return true; } // חדש
    std::string toString() const override { return std::string(1, color) + "K"; }
};

class Rook : public Piece {
public:
    Rook(char c) : Piece(c) {}
    bool isValidShape(const Position& from, const Position& to) const override {
        if (from.row == to.row && from.col == to.col) return false;
        return from.row == to.row || from.col == to.col;
    }
    bool isSliding() const override { return true; }
    std::string toString() const override { return std::string(1, color) + "R"; }
};

class Bishop : public Piece {
public:
    Bishop(char c) : Piece(c) {}
    bool isValidShape(const Position& from, const Position& to) const override {
        int dr = std::abs(to.row - from.row);
        int dc = std::abs(to.col - from.col);
        return dr == dc && dr > 0;
    }
    bool isSliding() const override { return true; }
    std::string toString() const override { return std::string(1, color) + "B"; }
};

class Queen : public Piece {
public:
    Queen(char c) : Piece(c) {}
    bool isValidShape(const Position& from, const Position& to) const override {
        int dr = std::abs(to.row - from.row);
        int dc = std::abs(to.col - from.col);
        if (dr == 0 && dc == 0) return false;
        return (from.row == to.row || from.col == to.col) || (dr == dc);
    }
    bool isSliding() const override { return true; }
    std::string toString() const override { return std::string(1, color) + "Q"; }
};

class Knight : public Piece {
public:
    Knight(char c) : Piece(c) {}
    bool isValidShape(const Position& from, const Position& to) const override {
        int dr = std::abs(to.row - from.row);
        int dc = std::abs(to.col - from.col);
        return (dr == 1 && dc == 2) || (dr == 2 && dc == 1);
    }
    // isSliding נשאר false (ברירת מחדל) - פרש קופץ מעל הכל
    std::string toString() const override { return std::string(1, color) + "N"; }
};

class Pawn : public Piece {
public:
    Pawn(char c) : Piece(c) {}
    bool isValidShape(const Position& from, const Position& to) const override {
        // הזזה רגילה: קדימה, עמודה זהה, צעד יחיד בלבד
        int dr = to.row - from.row;
        int dc = to.col - from.col;
        int direction = (color == 'w') ? -1 : 1;
        return dc == 0 && dr == direction;
    }
    bool isValidCapture(const Position& from, const Position& to) const override {
        // אכילה: קדימה-אלכסון בלבד, צעד יחיד
        int dr = to.row - from.row;
        int dc = std::abs(to.col - from.col);
        int direction = (color == 'w') ? -1 : 1;
        return dc == 1 && dr == direction;
    }
    std::string toString() const override { return std::string(1, color) + "P"; }
};
#endif