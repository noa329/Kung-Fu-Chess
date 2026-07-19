#ifndef REAL_TIME_ARBITER_H
#define REAL_TIME_ARBITER_H
#include <vector>
#include <memory>
#include "Board.hpp"
#include "Piece.hpp"
#include "Position.hpp"
#include "PieceKind.hpp"

struct PendingMove {
    Position from;
    Position to;
    long long startTime;
    long long arrivalTime;
    std::shared_ptr<Piece> piece;
};

struct AirborneState {
    Position pos;
    long long endTime;
    std::shared_ptr<Piece> piece;
};

struct CaptureEvent {
    Position at;
    char capturedColor;
    bool wasKing;
    PieceKind capturedKind;
};

struct MoveProgress {
    Position from;
    Position to;
    double progress; // 0.0 (just scheduled) .. 1.0 (arriving)
};

struct RestingState {
    Position pos;
    long long endTime;
    bool isLongRest;
};

class RealTimeArbiter {
    Board& board;
    long long currentTime = 0;
    std::vector<PendingMove> pendingMoves;
    std::vector<AirborneState> airbornePieces;
    std::vector<RestingState> restingPieces;

    static const long long CELL_TRAVEL_TIME_MS = 1000;
    static const long long CAPTURE_DURATION_MS = 1000;
    static const long long JUMP_DURATION_MS = 1000;
    static const long long LONG_REST_MS = 800;
    static const long long SHORT_REST_MS = 500;

    long long calculateTravelTime(const Position& from, const Position& to, bool isCapture) const;
    void resolveArrival(const PendingMove& pm, std::vector<CaptureEvent>& events);
    void finalizeReadyMoves(std::vector<CaptureEvent>& events);
    void finalizeAirborne();
    void finalizeResting();

public:
    explicit RealTimeArbiter(Board& b) : board(b) {}

    bool hasPendingMoveFrom(const Position& pos) const;
    bool hasPendingMoveTo(const Position& pos) const;
    bool isAirborne(const Position& pos) const;
    bool isResting(const Position& pos, bool* out_isLongRest = nullptr) const;

    bool getMoveProgress(const Position& pos, MoveProgress& out) const;

    void scheduleMove(const Position& from, const Position& to, std::shared_ptr<Piece> piece, bool isCapture);
    void scheduleJump(const Position& pos, std::shared_ptr<Piece> piece);

    std::vector<CaptureEvent> advance(int ms);
};
#endif
