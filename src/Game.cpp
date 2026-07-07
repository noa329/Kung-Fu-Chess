#include "Game.h"

bool Game::hasPendingMoveFrom(const Position& pos) const {
    for (const auto& pm : pendingMoves) {
        if (pm.from == pos) return true;
    }
    return false;
}

void Game::finalizeReadyMoves() {
    for (auto it = pendingMoves.begin(); it != pendingMoves.end(); ) {
        if (it->arrivalTime <= currentTime) {
            board.setCell(it->to.row, it->to.col, it->piece);
            board.setCell(it->from.row, it->from.col, nullptr);
            it = pendingMoves.erase(it);
        } else {
            ++it;
        }
    }
}

void Game::wait(int ms) {
    currentTime += ms;
    finalizeReadyMoves();
}

void Game::click(int x, int y) {
    Position pos = board.pixelToGrid(x, y);
    if (!board.isInside(pos.row, pos.col)) return;

    auto cell = board.getCell(pos.row, pos.col);

    if (selected.row == -1) {
        if (cell && !hasPendingMoveFrom(pos)) selected = pos;
        return;
    }

    auto selectedPiece = board.getCell(selected.row, selected.col);

    if (cell) {
        if (cell->getColor() == selectedPiece->getColor()) {
            if (!hasPendingMoveFrom(pos)) selected = pos; // כלי ידידותי - מחליפים בחירה
            return;
        }
        // כלי יריב - ניסיון אכילה
        if (!selectedPiece->isValidCapture(selected, pos)) return;
        if (selectedPiece->isSliding() && !board.isPathClear(selected, pos)) return;

        pendingMoves.push_back({selected, pos, currentTime + MOVE_DURATION_MS, selectedPiece});
        selected = {-1, -1};
        return;
    }

    // תא ריק - הזזה רגילה
    if (!selectedPiece->isValidShape(selected, pos)) return;
    if (selectedPiece->isSliding() && !board.isPathClear(selected, pos)) return;

    pendingMoves.push_back({selected, pos, currentTime + MOVE_DURATION_MS, selectedPiece});
    selected = {-1, -1};
}