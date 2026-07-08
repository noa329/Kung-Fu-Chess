#ifndef GAME_H
#define GAME_H
#include "Board.h"
#include "Position.h"
#include <vector>
#include <memory>

struct PendingMove {
    Position from;
    Position to;
    long long arrivalTime;
    std::shared_ptr<Piece> piece;
};

class Game {
    Board board;
    Position selected = {-1, -1};
    long long currentTime = 0;
    std::vector<PendingMove> pendingMoves;
    static const long long CELL_TRAVEL_TIME_MS = 1000;

    bool hasPendingMoveFrom(const Position& pos) const;
    bool hasPendingMoveTo(const Position& pos) const;          
    bool hasPendingMoveOfOppositeColor(char color) const;
    void finalizeReadyMoves();
    void resolveArrival(const PendingMove& pm);                
    bool isMovementLegal(std::shared_ptr<Piece> piece, const Position& from,
                          const Position& to, bool isCapture) const;
    long long calculateTravelTime(const Position& from, const Position& to) const;
    void scheduleMove(const Position& from, const Position& to, std::shared_ptr<Piece> piece);
public:
    void loadBoard(const std::vector<std::vector<std::string>>& grid) { board.setGrid(grid); }
    void click(int x, int y);
    void wait(int ms);
    void printBoard() const { board.print(); }
};
#endif