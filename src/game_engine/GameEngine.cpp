#include "GameEngine.hpp"
#include "Pieces.hpp"
#include "RuleEngine.hpp"
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
    }
}

void GameEngine::setPlayerNames(const std::string& whiteName, const std::string& blackName) {
    whiteName_ = whiteName;
    blackName_ = blackName;
}

bool GameEngine::isMovementLegal(std::shared_ptr<Piece> piece, const Position& from,
                                  const Position& to, bool isCapture) const {
    RuleEngine engine(board);
    if (!engine.isLegal(piece, from, to, isCapture)) return false;
    if (arbiter.hasPendingMoveTo(to)) return false;
    return true;
}

void GameEngine::wait(int ms) {
    clock_ += ms;
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
    moveHistory_.push_back({clock_, piece->getColor(), squareName(pos, board.getRowCount())});
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
            moveHistory_.push_back({clock_, selectedPiece->getColor(),
                squareName(selected, board.getRowCount()) + squareName(pos, board.getRowCount())});
            selected = {-1, -1};
        }
        return;
    }

    if (isMovementLegal(selectedPiece, selected, pos, /*isCapture=*/false)) {
        arbiter.scheduleMove(selected, pos, selectedPiece, /*isCapture=*/false);
        moveHistory_.push_back({clock_, selectedPiece->getColor(),
            squareName(selected, board.getRowCount()) + squareName(pos, board.getRowCount())});
        selected = {-1, -1};
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
                }
            }
            snap.cellStates[r][c] = state;
        }
    }
    return snap;
}
