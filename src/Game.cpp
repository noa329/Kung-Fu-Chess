#include "Game.h"

void Game::click(int x, int y) {
    Position pos = board.pixelToGrid(x, y);
    if (!board.isInside(pos.row, pos.col)) return;

    auto cell = board.getCell(pos.row, pos.col);

    if (selected.row == -1) {
        if (cell) selected = pos;
        return;
    }

    if (cell) {
        selected = pos; // clicking another piece replaces the selection
        return;
    }

    auto selectedPiece = board.getCell(selected.row, selected.col);
    if (!selectedPiece->isValidShape(selected, pos)) return; // illegal shape - ignore

    board.setCell(pos.row, pos.col, selectedPiece);
    board.setCell(selected.row, selected.col, nullptr);
    selected = {-1, -1};
}