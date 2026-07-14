#ifndef PIECES_H
#define PIECES_H
#include "Piece.hpp"

class King : public Piece {
public:
    King(char c) : Piece(c) {}
    PieceKind getKind() const override { return PieceKind::King; }
    std::string toString() const override { return std::string(1, color) + "K"; }
};

class Rook : public Piece {
public:
    Rook(char c) : Piece(c) {}
    PieceKind getKind() const override { return PieceKind::Rook; }
    std::string toString() const override { return std::string(1, color) + "R"; }
};

class Bishop : public Piece {
public:
    Bishop(char c) : Piece(c) {}
    PieceKind getKind() const override { return PieceKind::Bishop; }
    std::string toString() const override { return std::string(1, color) + "B"; }
};

class Queen : public Piece {
public:
    Queen(char c) : Piece(c) {}
    PieceKind getKind() const override { return PieceKind::Queen; }
    std::string toString() const override { return std::string(1, color) + "Q"; }
};

class Knight : public Piece {
public:
    Knight(char c) : Piece(c) {}
    PieceKind getKind() const override { return PieceKind::Knight; }
    std::string toString() const override { return std::string(1, color) + "N"; }
};

class Pawn : public Piece {
public:
    Pawn(char c) : Piece(c) {}
    PieceKind getKind() const override { return PieceKind::Pawn; }
    std::string toString() const override { return std::string(1, color) + "P"; }
};
#endif
