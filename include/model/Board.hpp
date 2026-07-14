#ifndef BOARD_H
#define BOARD_H
#include <vector>
#include <memory>
#include <string>
#include "Position.hpp"
#include "Piece.hpp"

class Board {
    std::vector<std::vector<std::shared_ptr<Piece>>> grid;
    int cellPixelSize = 100;
public:
    Board() = default;
    
    void setGrid(const std::vector<std::vector<std::string>>& tokens);
    bool isInside(int r, int c) const;
    std::shared_ptr<Piece> getCell(int r, int c) const;
    void setCell(int r, int c, std::shared_ptr<Piece> piece);
    bool isPathClear(const Position& from, const Position& to) const;
    int getRowCount() const; 
    int getColCount() const;
};
#endif