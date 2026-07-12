#include "Game.hpp"
#include "Pieces.hpp"
#include "MovementRules.hpp"
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

bool Game::isAirborne(const Position& pos) const {
    for (const auto& a : airbornePieces) {
        if (a.pos == pos) return true;
    }
    return false;
}

void Game::resolveArrival(const PendingMove& pm) {
    auto occupant = board.getCell(pm.to.row, pm.to.col);

    if (occupant && occupant->getColor() == pm.piece->getColor()) {
        return; // כלי ידידותי - הגנה, לא אמור לקרות
    }

    if (occupant && isAirborne(pm.to) && occupant->getColor() != pm.piece->getColor()) {
        // הכלי המקפיץ תופס את התוקף באוויר: התוקף מוסר לגמרי מהלוח, המקפיץ נשאר
        board.setCell(pm.from.row, pm.from.col, nullptr); // תוקן: התוקף נמחק
        return;
    }

    if (occupant && occupant->isKing()) {
        gameOver = true;
    }

    board.setCell(pm.to.row, pm.to.col, pm.piece);
    board.setCell(pm.from.row, pm.from.col, nullptr);

    if (pm.piece->isPawn()) {
        int lastRow = (pm.piece->getColor() == 'w') ? 0 : board.getRowCount() - 1;
        if (pm.to.row == lastRow) {
            board.setCell(pm.to.row, pm.to.col, std::make_shared<Queen>(pm.piece->getColor()));
        }
    }
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

void Game::finalizeAirborne() {
    for (auto it = airbornePieces.begin(); it != airbornePieces.end(); ) {
        if (it->endTime <= currentTime) {
            it = airbornePieces.erase(it);
        } else {
            ++it;
        }
    }
}

void Game::wait(int ms) {
    currentTime += ms;
    finalizeReadyMoves();  
    finalizeAirborne();
}

long long Game::calculateTravelTime(const Position& from, const Position& to, bool isCapture) const {
    if (isCapture) {
        return CAPTURE_DURATION_MS; 
    }
    int dr = std::abs(to.row - from.row);
    int dc = std::abs(to.col - from.col);
    int cellDistance = std::max(dr, dc);
    return cellDistance * CELL_TRAVEL_TIME_MS;
}

bool Game::isMovementLegal(std::shared_ptr<Piece> piece, const Position& from,
                            const Position& to, bool isCapture) const {
    int rows = board.getRowCount();
    PieceKind kind = piece->getKind();
    char color = piece->getColor();
    bool validShape = isCapture ? MovementRules::isValidCapture(kind, color, from, to, rows)
                                 : MovementRules::isValidShape(kind, color, from, to, rows);
    if (!validShape) return false;
    if (MovementRules::isSliding(kind) && !board.isPathClear(from, to)) return false;
    if (hasPendingMoveOfOppositeColor(piece->getColor())) return false;
    if (hasPendingMoveTo(to)) return false;
    return true;
}

void Game::scheduleMove(const Position& from, const Position& to, std::shared_ptr<Piece> piece, bool isCapture) {
    long long arrival = currentTime + calculateTravelTime(from, to, isCapture);
    pendingMoves.push_back({from, to, arrival, piece});
    selected = {-1, -1};
}

void Game::jump(int x, int y) {
    if (gameOver) return;
    Position pos = board.pixelToGrid(x, y);
    if (!board.isInside(pos.row, pos.col)) return;

    auto piece = board.getCell(pos.row, pos.col);
    if (!piece) return;
    if (hasPendingMoveFrom(pos)) return;
    if (isAirborne(pos)) return;

    airbornePieces.push_back({pos, currentTime + JUMP_DURATION_MS, piece});
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
            scheduleMove(selected, pos, selectedPiece, /*isCapture=*/true);
        }
        return;
    }

    if (isMovementLegal(selectedPiece, selected, pos, /*isCapture=*/false)) {
        scheduleMove(selected, pos, selectedPiece, /*isCapture=*/false);
    }
}