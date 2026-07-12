#include "Game.hpp"
#include "Pieces.hpp"
#include "RuleEngine.hpp"
#include <algorithm>
#include <cstdlib>

void Game::applyCaptureEvents(const std::vector<CaptureEvent>& events) {
    // GameEngine-level decision: אכילת מלך מסיימת את המשחק. הדיווח עצמו
    // (מי נאכל, האם זה היה מלך) מגיע מ-RealTimeArbiter; ההחלטה מה המשמעות
    // של זה שייכת לכאן.
    for (const auto& e : events) {
        if (e.wasKing) gameOver = true;
    }
}

bool Game::isMovementLegal(std::shared_ptr<Piece> piece, const Position& from,
                            const Position& to, bool isCapture) const {
    // חוקיות שחמט טהורה (צורה + מסלול פנוי) מואצלת ל-RuleEngine.
    RuleEngine engine(board);
    if (!engine.isLegal(piece, from, to, isCapture)) return false;

    // מצב תנועות ממתינות (RealTimeArbiter).
    if (arbiter.hasPendingMoveOfOppositeColor(piece->getColor())) return false;
    if (arbiter.hasPendingMoveTo(to)) return false;
    return true;
}

void Game::wait(int ms) {
    auto events = arbiter.advance(ms);
    applyCaptureEvents(events);
}

void Game::jump(int x, int y) {
    if (gameOver) return;
    Position pos = board.pixelToGrid(x, y);
    if (!board.isInside(pos.row, pos.col)) return;

    auto piece = board.getCell(pos.row, pos.col);
    if (!piece) return;
    if (arbiter.hasPendingMoveFrom(pos)) return;
    if (arbiter.isAirborne(pos)) return;

    arbiter.scheduleJump(pos, piece);
    selected = {-1, -1};
}

void Game::click(int x, int y) {
    if (gameOver) return;

    Position pos = board.pixelToGrid(x, y);
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
