#include "GameEngine.hpp"
#include "Pieces.hpp"
#include "RuleEngine.hpp"
#include "SoundManager.hpp"
#include <algorithm>
#include <cstdlib>

namespace {

int pieceValue(PieceKind kind) {
    switch (kind) {
        case PieceKind::Pawn:   return 1;
        case PieceKind::Knight: return 3;
        case PieceKind::Bishop: return 3;
        case PieceKind::Rook:   return 5;
        case PieceKind::Queen:  return 9;
        case PieceKind::King:   return 0; // king captures end the game via wasKing, not scored
    }
    return 0;
}

std::string squareName(const Position& pos, int rowCount) {
    return std::string(1, char('a' + pos.col)) + std::to_string(rowCount - pos.row);
}

} // namespace

void GameEngine::applyCaptureEvents(const std::vector<CaptureEvent>& events) {
    for (const auto& e : events) {
        if (e.wasKing) gameOver = true;
        int value = pieceValue(e.capturedKind);
        if (e.capturedColor == 'w') blackScore_ += value; else whiteScore_ += value;
        activeCaptures_.push_back({e.at, e.capturedColor, e.wasKing, clock_, clock_ + CAPTURE_EFFECT_MS});
        SoundManager::instance().playCaptureSound();
    }
}

void GameEngine::pruneCaptureFlashes() {
    for (auto it = activeCaptures_.begin(); it != activeCaptures_.end(); ) {
        if (it->endTime <= clock_) {
            it = activeCaptures_.erase(it);
        } else {
            ++it;
        }
    }
}

void GameEngine::setPlayerNames(const std::string& whiteName, const std::string& blackName) {
    whiteName_ = whiteName;
    blackName_ = blackName;
}

void GameEngine::setRestDurations(long long longRestMs, long long shortRestMs) {
    arbiter.setRestDurations(longRestMs, shortRestMs);
}

bool GameEngine::isMovementLegal(std::shared_ptr<Piece> piece, const Position& from,
                                  const Position& to, bool isCapture) const {
    RuleEngine engine(board);
    if (!engine.isLegal(piece, from, to, isCapture)) return false;
    if (arbiter.hasPendingMoveTo(to)) return false;
    if (arbiter.isResting(from)) return false;
    return true;
}

void GameEngine::wait(int ms) {
    clock_ += ms;
    auto events = arbiter.advance(ms);
    applyCaptureEvents(events);
    pruneCaptureFlashes();
}

void GameEngine::jump(const Position& pos) {
    if (gameOver) return;
    if (!board.isInside(pos.row, pos.col)) return;

    auto piece = board.getCell(pos.row, pos.col);
    if (!piece) return;
    if (arbiter.hasPendingMoveFrom(pos)) return;
    if (arbiter.isAirborne(pos)) return;
    if (arbiter.isResting(pos)) return;

    arbiter.scheduleJump(pos, piece);
    moveHistory_.push_back({clock_, piece->getColor(), squareName(pos, board.getRowCount())});
    selected = {-1, -1};
    SoundManager::instance().playJumpSound();
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
            moveHistory_.push_back({clock_, selectedPiece->getColor(),
                squareName(selected, board.getRowCount()) + squareName(pos, board.getRowCount())});
            selected = {-1, -1};
            // The capture sound itself plays later, from applyCaptureEvents(),
            // once the piece actually arrives and the capture resolves - this
            // is the travel-time "the piece just set off" sound.
            SoundManager::instance().playMoveSound();
        }
        return;
    }

    if (isMovementLegal(selectedPiece, selected, pos, /*isCapture=*/false)) {
        arbiter.scheduleMove(selected, pos, selectedPiece, /*isCapture=*/false);
        moveHistory_.push_back({clock_, selectedPiece->getColor(),
            squareName(selected, board.getRowCount()) + squareName(pos, board.getRowCount())});
        selected = {-1, -1};
        SoundManager::instance().playMoveSound();
    }
}

GameSnapshot GameEngine::snapshot() const {
    GameSnapshot snap;
    snap.selected = selected;
    snap.gameOver = gameOver;
    snap.whiteName = whiteName_;
    snap.blackName = blackName_;
    snap.whiteScore = whiteScore_;
    snap.blackScore = blackScore_;
    for (const auto& m : moveHistory_) {
        if (m.color == 'w') snap.whiteMoves.push_back(m); else snap.blackMoves.push_back(m);
    }
    int rows = board.getRowCount();
    int cols = board.getColCount();
    snap.boardTokens.resize(rows);
    snap.cellStates.resize(rows);
    snap.moveTargets.resize(rows);
    snap.moveProgress.resize(rows);
    for (int r = 0; r < rows; ++r) {
        snap.boardTokens[r].resize(cols);
        snap.cellStates[r].resize(cols);
        snap.moveTargets[r].resize(cols);
        snap.moveProgress[r].resize(cols, 0.0);
        for (int c = 0; c < cols; ++c) {
            auto piece = board.getCell(r, c);
            snap.boardTokens[r][c] = piece ? piece->toString() : ".";

            std::string state = "idle";
            if (piece) {
                Position pos{r, c};
                MoveProgress mp;
                if (arbiter.getMoveProgress(pos, mp)) {
                    state = "move";
                    snap.moveTargets[r][c] = mp.to;
                    snap.moveProgress[r][c] = mp.progress;
                } else if (arbiter.isAirborne(pos)) {
                    state = "jump";
                } else {
                    bool isLongRest = false;
                    if (arbiter.isResting(pos, &isLongRest)) {
                        state = isLongRest ? "long_rest" : "short_rest";
                    }
                }
            }
            snap.cellStates[r][c] = state;
        }
    }
    for (const auto& ac : activeCaptures_) {
        long long duration = ac.endTime - ac.startTime;
        double progress = (duration > 0)
            ? static_cast<double>(clock_ - ac.startTime) / static_cast<double>(duration)
            : 1.0;
        if (progress < 0.0) progress = 0.0;
        if (progress > 1.0) progress = 1.0;
        snap.captureFlashes.push_back({ac.at, ac.capturedColor, ac.wasKing, progress});
    }
    return snap;
}
