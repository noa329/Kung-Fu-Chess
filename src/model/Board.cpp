#include "model/Board.hpp"
#include "model/PieceFactory.hpp"

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

bool Board::isInside(int r, int c) const {
    return r >= 0 && r < (int)grid.size() && c >= 0 && c < (int)grid[0].size();
}

std::shared_ptr<Piece> Board::getCell(int r, int c) const {
    return grid[r][c];
}

void Board::setCell(int r, int c, std::shared_ptr<Piece> piece) {
    grid[r][c] = piece;
}

bool Board::isPathClear(const Position& from, const Position& to) const {
    int dr = (to.row > from.row) ? 1 : (to.row < from.row) ? -1 : 0;
    int dc = (to.col > from.col) ? 1 : (to.col < from.col) ? -1 : 0;

    int r = from.row + dr;
    int c = from.col + dc;
    while (r != to.row || c != to.col) {
        if (getCell(r, c) != nullptr) return false; // יש כלי בדרך
        r += dr;
        c += dc;
    }
    return true; // המסלול פנוי (לא כולל from ו-to עצמם)
}

int Board::getRowCount() const {
    return (int)grid.size();
}

int Board::getColCount() const {
    return grid.empty() ? 0 : (int)grid[0].size();
}