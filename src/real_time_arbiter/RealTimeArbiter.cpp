#include "RealTimeArbiter.hpp"
#include "Pieces.hpp"
#include <algorithm>
#include <cstdlib>

bool RealTimeArbiter::hasPendingMoveFrom(const Position& pos) const {
    for (const auto& pm : pendingMoves) {
        if (pm.from == pos) return true;
    }
    return false;
}

bool RealTimeArbiter::hasPendingMoveTo(const Position& pos) const {
    for (const auto& pm : pendingMoves) {
        if (pm.to == pos) return true;
    }
    return false;
}

bool RealTimeArbiter::isAirborne(const Position& pos) const {
    for (const auto& a : airbornePieces) {
        if (a.pos == pos) return true;
    }
    return false;
}

bool RealTimeArbiter::isResting(const Position& pos, bool* out_isLongRest) const {
    for (const auto& r : restingPieces) {
        if (r.pos == pos) {
            if (out_isLongRest) *out_isLongRest = r.isLongRest;
            return true;
        }
    }
    return false;
}

long long RealTimeArbiter::calculateTravelTime(const Position& from, const Position& to, bool isCapture) const {
    if (isCapture) {
        return CAPTURE_DURATION_MS;
    }
    int dr = std::abs(to.row - from.row);
    int dc = std::abs(to.col - from.col);
    int cellDistance = std::max(dr, dc);
    return cellDistance * CELL_TRAVEL_TIME_MS;
}

bool RealTimeArbiter::getMoveProgress(const Position& pos, MoveProgress& out) const {
    for (const auto& pm : pendingMoves) {
        if (pm.from == pos) {
            long long duration = pm.arrivalTime - pm.startTime;
            double p = (duration > 0)
                ? static_cast<double>(currentTime - pm.startTime) / static_cast<double>(duration)
                : 1.0;
            if (p < 0.0) p = 0.0;
            if (p > 1.0) p = 1.0;
            out.from = pm.from;
            out.to = pm.to;
            out.progress = p;
            return true;
        }
    }
    return false;
}

void RealTimeArbiter::scheduleMove(const Position& from, const Position& to, std::shared_ptr<Piece> piece, bool isCapture) {
    long long arrival = currentTime + calculateTravelTime(from, to, isCapture);
    pendingMoves.push_back({from, to, currentTime, arrival, piece});
}

void RealTimeArbiter::scheduleJump(const Position& pos, std::shared_ptr<Piece> piece) {
    airbornePieces.push_back({pos, currentTime + JUMP_DURATION_MS, piece});
}

void RealTimeArbiter::resolveArrival(const PendingMove& pm, std::vector<CaptureEvent>& events) {
    auto occupant = board.getCell(pm.to.row, pm.to.col);

    if (occupant && occupant->getColor() == pm.piece->getColor()) {
        return; 
    }

    if (occupant && isAirborne(pm.to) && occupant->getColor() != pm.piece->getColor()) {
        board.setCell(pm.from.row, pm.from.col, nullptr);
        return;
    }

    if (occupant) {
        events.push_back({pm.to, occupant->getColor(), occupant->isKing(), occupant->getKind()});
    }

    board.setCell(pm.to.row, pm.to.col, pm.piece);
    board.setCell(pm.from.row, pm.from.col, nullptr);

    if (pm.piece->isPawn()) {
        int lastRow = (pm.piece->getColor() == 'w') ? 0 : board.getRowCount() - 1;
        if (pm.to.row == lastRow) {
            board.setCell(pm.to.row, pm.to.col, std::make_shared<Queen>(pm.piece->getColor()));
        }
    }

    restingPieces.push_back({pm.to, currentTime + LONG_REST_MS, /*isLongRest=*/true});
}

void RealTimeArbiter::finalizeReadyMoves(std::vector<CaptureEvent>& events) {
    for (auto it = pendingMoves.begin(); it != pendingMoves.end(); ) {
        if (it->arrivalTime <= currentTime) {
            resolveArrival(*it, events);
            it = pendingMoves.erase(it);
        } else {
            ++it;
        }
    }
}

void RealTimeArbiter::finalizeAirborne() {
    for (auto it = airbornePieces.begin(); it != airbornePieces.end(); ) {
        if (it->endTime <= currentTime) {
            restingPieces.push_back({it->pos, currentTime + SHORT_REST_MS, /*isLongRest=*/false});
            it = airbornePieces.erase(it);
        } else {
            ++it;
        }
    }
}

void RealTimeArbiter::finalizeResting() {
    for (auto it = restingPieces.begin(); it != restingPieces.end(); ) {
        if (it->endTime <= currentTime) {
            it = restingPieces.erase(it);
        } else {
            ++it;
        }
    }
}

std::vector<CaptureEvent> RealTimeArbiter::advance(int ms) {
    currentTime += ms;
    std::vector<CaptureEvent> events;
    finalizeReadyMoves(events);
    finalizeAirborne();
    finalizeResting();
    return events;
}
