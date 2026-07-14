#include "GameEngine.hpp"
#include "Pieces.hpp"
#include "RuleEngine.hpp"
#include <algorithm>
#include <cstdlib>

void GameEngine::applyCaptureEvents(const std::vector<CaptureEvent>& events) {
    for (const auto& e : events) {
        if (e.wasKing) gameOver = true;
    }
}

bool GameEngine::isMovementLegal(std::shared_ptr<Piece> piece, const Position& from,
                                  const Position& to, bool isCapture) const {
    RuleEngine engine(board);
    if (!engine.isLegal(piece, from, to, isCapture)) return false;
    if (arbiter.hasPendingMoveTo(to)) return false;
    return true;
}

void GameEngine::wait(int ms) {
    auto events = arbiter.advance(ms);
    applyCaptureEvents(events);
}

void GameEngine::jump(const Position& pos) {
    if (gameOver) return;
    if (!board.isInside(pos.row, pos.col)) return;

    auto piece = board.getCell(pos.row, pos.col);
    if (!piece) return;
    if (arbiter.hasPendingMoveFrom(pos)) return;
    if (arbiter.isAirborne(pos)) return;

    arbiter.scheduleJump(pos, piece);
    selected = {-1, -1};
}

void GameEngine::select(const Position& pos) {
    if (gameOver) return;
    if (!board.isInside(pos.row, pos.col)) return;
    if (arbiter.hasPendingMoveFrom(pos)) return;

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
            arbiter.scheduleMove(selected, pos, selectedPiece, /*isCapture=*/true);
            selected = {-1, -1};
        }
        return;
    }

    if (isMovementLegal(selectedPiece, selected, pos, /*isCapture=*/false)) {
        arbiter.scheduleMove(selected, pos, selectedPiece, /*isCapture=*/false);
        selected = {-1, -1};
    }
}

GameSnapshot GameEngine::snapshot() const {
    GameSnapshot snap;
    snap.selected = selected;
    snap.gameOver = gameOver;
    int rows = board.getRowCount();
    int cols = board.getColCount();
    snap.boardTokens.resize(rows);
    for (int r = 0; r < rows; ++r) {
        snap.boardTokens[r].resize(cols);
        for (int c = 0; c < cols; ++c) {
            auto piece = board.getCell(r, c);
            snap.boardTokens[r][c] = piece ? piece->toString() : ".";
        }
    }
    return snap;
}
