#include "Game.h"
#include <algorithm>
#include <cstdlib>

bool Game::hasPendingMoveFrom(const Position& pos) const {
    for (const auto& pm : pendingMoves) {
        if (pm.from == pos) return true;
    }
    return false;
}

bool Game::hasPendingMoveTo(const Position& pos) const {
    for (const auto& pm : pendingMoves) {
        if (pm.to == pos) return true;
    }
    return false;
}

bool Game::hasPendingMoveOfOppositeColor(char color) const {
    for (const auto& pm : pendingMoves) {
        if (pm.piece->getColor() != color) return true;
    }
    return false;
}

void Game::resolveArrival(const PendingMove& pm) {
    auto occupant = board.getCell(pm.to.row, pm.to.col);
    if (occupant && occupant->getColor() == pm.piece->getColor()) {
        return; // הגנה - לא אמור לקרות
    }
    if (occupant && occupant->isKing()) {
        gameOver = true; // נאכל מלך - המשחק נגמר
    }
    board.setCell(pm.to.row, pm.to.col, pm.piece);
    board.setCell(pm.from.row, pm.from.col, nullptr);
}

void Game::finalizeReadyMoves() {
    for (auto it = pendingMoves.begin(); it != pendingMoves.end(); ) {
        if (it->arrivalTime <= currentTime) {
            resolveArrival(*it);
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

long long Game::calculateTravelTime(const Position& from, const Position& to) const {
    int dr = std::abs(to.row - from.row);
    int dc = std::abs(to.col - from.col);
    int cellDistance = std::max(dr, dc);
    return cellDistance * CELL_TRAVEL_TIME_MS;
}

bool Game::isMovementLegal(std::shared_ptr<Piece> piece, const Position& from,
                            const Position& to, bool isCapture) const {
    bool validShape = isCapture ? piece->isValidCapture(from, to) : piece->isValidShape(from, to);
    if (!validShape) return false;
    if (piece->isSliding() && !board.isPathClear(from, to)) return false;
    if (hasPendingMoveOfOppositeColor(piece->getColor())) return false;
    if (hasPendingMoveTo(to)) return false;
    return true;
}

void Game::scheduleMove(const Position& from, const Position& to, std::shared_ptr<Piece> piece) {
    long long arrival = currentTime + calculateTravelTime(from, to);
    pendingMoves.push_back({from, to, arrival, piece});
    selected = {-1, -1};
}

void Game::click(int x, int y) {
    if (gameOver) return; 
    Position pos = board.pixelToGrid(x, y);
    if (!board.isInside(pos.row, pos.col)) return;
    if (hasPendingMoveFrom(pos)) return;

    auto cell = board.getCell(pos.row, pos.col);

    if (selected.row == -1) {
        if (cell) selected = pos;
        return;
    }

    auto selectedPiece = board.getCell(selected.row, selected.col);

    if (cell) {
        if (cell->getColor() == selectedPiece->getColor()) {
            selected = pos;
            return;
        }
        if (isMovementLegal(selectedPiece, selected, pos, /*isCapture=*/true)) {
            scheduleMove(selected, pos, selectedPiece);
        }
        return;
    }

    if (isMovementLegal(selectedPiece, selected, pos, /*isCapture=*/false)) {
        scheduleMove(selected, pos, selectedPiece);
    }
}