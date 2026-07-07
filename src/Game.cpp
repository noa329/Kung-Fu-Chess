#include "Game.h"

void Game::click(int x, int y) {
    Position pos = board.pixelToGrid(x, y);
    if (!board.isInside(pos.row, pos.col)) return;

    auto cell = board.getCell(pos.row, pos.col);

    if (selected.row == -1) {
        if (cell) selected = pos;
        return;
    }

    auto selectedPiece = board.getCell(selected.row, selected.col);

    if (cell) {
        if (cell->getColor() == selectedPiece->getColor()) {
            // כלי ידידותי - מחליפים בחירה
            selected = pos;
            return;
        }
        // כלי יריב - ניסיון אכילה
        if (!selectedPiece->isValidShape(selected, pos)) return;
        if (selectedPiece->isSliding() && !board.isPathClear(selected, pos)) return;

        board.setCell(pos.row, pos.col, selectedPiece); // אכילה: הכלי היריב מוחלף
        board.setCell(selected.row, selected.col, nullptr);
        selected = {-1, -1};
        return;
    }

    // תא ריק - הזזה רגילה
    if (!selectedPiece->isValidShape(selected, pos)) return;
    if (selectedPiece->isSliding() && !board.isPathClear(selected, pos)) return;

    board.setCell(pos.row, pos.col, selectedPiece);
    board.setCell(selected.row, selected.col, nullptr);
    selected = {-1, -1};
}