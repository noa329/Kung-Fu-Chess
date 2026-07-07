#include "Board.h"
#include "PieceFactory.h"
#include <iostream>

void Board::setGrid(const std::vector<std::vector<std::string>>& tokens) {
    grid.clear();
    for (const auto& row : tokens) {
        std::vector<std::shared_ptr<Piece>> pieceRow;
        for (const auto& token : row) {
            pieceRow.push_back(createPiece(token));
        }
        grid.push_back(pieceRow);
    }
}

Position Board::pixelToGrid(int x, int y) const {
    int r = (y >= 0) ? y / 100 : (y - 99) / 100;
    int c = (x >= 0) ? x / 100 : (x - 99) / 100;
    return {r, c};
}

bool Board::isInside(int r, int c) const {
    return r >= 0 && r < (int)grid.size() && c >= 0 && c < (int)grid[0].size();
}

std::shared_ptr<Piece> Board::getCell(int r, int c) const {
    return grid[r][c];
}

void Board::setCell(int r, int c, std::shared_ptr<Piece> piece) {
    grid[r][c] = piece;
}

void Board::print() const {
    for (size_t i = 0; i < grid.size(); ++i) {
        for (size_t j = 0; j < grid[i].size(); ++j) {
            std::cout << (grid[i][j] ? grid[i][j]->toString() : ".")
                       << (j == grid[i].size() - 1 ? "" : " ");
        }
        std::cout << std::endl;
    }
}