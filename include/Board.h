#ifndef BOARD_H
#define BOARD_H
#include <vector>
#include <memory>
#include <string>
#include "Position.h"
#include "Piece.h"

class Board {
    std::vector<std::vector<std::shared_ptr<Piece>>> grid;
public:
    void setGrid(const std::vector<std::vector<std::string>>& tokens);
    Position pixelToGrid(int x, int y) const;
    bool isInside(int r, int c) const;
    std::shared_ptr<Piece> getCell(int r, int c) const;
    void setCell(int r, int c, std::shared_ptr<Piece> piece);
    bool isPathClear(const Position& from, const Position& to) const; // חדש
    void print() const;
};
#endif