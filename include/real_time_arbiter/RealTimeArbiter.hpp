#ifndef REAL_TIME_ARBITER_H
#define REAL_TIME_ARBITER_H
#include <vector>
#include <memory>
#include "Board.hpp"
#include "Piece.hpp"
#include "Position.hpp"

// שכבת RealTimeArbiter (ראו טבלת בעלות השכבות):
// בעלות: אובייקטי Motion פעילים, קידום זמן מדומה, פתרון הגעה, ואירועי אכילה.
// אסור לה: חוקיות שחמט, קליקים, רינדור, או פירוש סקריפט.
//
// שימו לב: היא כן מותרת לגעת ב-Board (כדי בפועל להזיז כלים כשמהלך מגיע),
// אבל היא לא מחליטה "המשחק נגמר" - היא רק מדווחת CaptureEvent החוצה,
// וה-GameEngine (השכבה הבאה) יחליט מה המשמעות של אכילת מלך.

struct PendingMove {
    Position from;
    Position to;
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
};

class RealTimeArbiter {
    Board& board;
    long long currentTime = 0;
    std::vector<PendingMove> pendingMoves;
    std::vector<AirborneState> airbornePieces;

    static const long long CELL_TRAVEL_TIME_MS = 1000;
    static const long long CAPTURE_DURATION_MS = 1000;
    static const long long JUMP_DURATION_MS = 1000;

    long long calculateTravelTime(const Position& from, const Position& to, bool isCapture) const;
    void resolveArrival(const PendingMove& pm, std::vector<CaptureEvent>& events);
    void finalizeReadyMoves(std::vector<CaptureEvent>& events);
    void finalizeAirborne();

public:
    explicit RealTimeArbiter(Board& b) : board(b) {}

    bool hasPendingMoveFrom(const Position& pos) const;
    bool hasPendingMoveTo(const Position& pos) const;
    bool isAirborne(const Position& pos) const;

    void scheduleMove(const Position& from, const Position& to, std::shared_ptr<Piece> piece, bool isCapture);
    void scheduleJump(const Position& pos, std::shared_ptr<Piece> piece);

    // מקדם את הזמן המדומה ב-ms, פותר מהלכים/קפיצות שהגיע זמנן,
    // ומחזיר את אירועי האכילה שקרו בפרק הזמן הזה.
    std::vector<CaptureEvent> advance(int ms);
};
#endif
