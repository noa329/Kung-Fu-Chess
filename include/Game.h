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
    static const long long MOVE_DURATION_MS = 2000;

    bool hasPendingMoveFrom(const Position& pos) const;
    void finalizeReadyMoves();
public:
    void loadBoard(const std::vector<std::vector<std::string>>& grid) { board.setGrid(grid); }
    void click(int x, int y);
    void wait(int ms);
    void printBoard() const { board.print(); }
};
#endif