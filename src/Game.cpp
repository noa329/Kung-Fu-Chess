#include "Game.h"
#include <iostream>

void Game::click(int x, int y) {
    Position pos = board.pixelToGrid(x, y);
    if (!board.isInside(pos.row, pos.col)) return;

    std::string cell = board.getCell(pos.row, pos.col);

    if (selected.row == -1) {
        if (cell != ".") selected = pos; // בחירה
    } else {
        if (cell != ".") {
            selected = pos; // החלפת בחירה לכלי ידידותי
        } else {
            // הזזה: מעבירים את הכלי מה-selected למיקום החדש
            std::string piece = board.getCell(selected.row, selected.col);
            board.setCell(pos.row, pos.col, piece);
            board.setCell(selected.row, selected.col, ".");
            selected = {-1, -1};
        }
    }
}