#ifndef GAME_H
#define GAME_H
#include "Board.hpp"
#include "Position.hpp"
#include "RealTimeArbiter.hpp"
#include <vector>
#include <memory>

class Game {
    Board board;
    RealTimeArbiter arbiter;
    Position selected = {-1, -1};
    bool gameOver = false;

    bool isMovementLegal(std::shared_ptr<Piece> piece, const Position& from,
                          const Position& to, bool isCapture) const;
    void applyCaptureEvents(const std::vector<CaptureEvent>& events);
public:
    Game() : arbiter(board) {}
    void loadBoard(const std::vector<std::vector<std::string>>& grid) { board.setGrid(grid); }
    void click(int x, int y);
    void jump(int x, int y);
    void wait(int ms);
    void printBoard() const { board.print(); }
};
#endif
