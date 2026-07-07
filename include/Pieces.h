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
    std::string toString() const override { return std::string(1, color) + "K"; }
};

class Rook : public Piece {
public:
    Rook(char c) : Piece(c) {}
    bool isValidShape(const Position& from, const Position& to) const override {
        if (from.row == to.row && from.col == to.col) return false;
        return from.row == to.row || from.col == to.col;
    }
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
    std::string toString() const override { return std::string(1, color) + "N"; }
};
#endif