#ifndef GAME_ENGINE_H
#define GAME_ENGINE_H
#include "Board.hpp"
#include "Position.hpp"
#include "RealTimeArbiter.hpp"
#include <vector>
#include <string>
#include <memory>

struct GameSnapshot {
    std::vector<std::vector<std::string>> boardTokens; 
    Position selected;
    bool gameOver;
};

class GameEngine {
    Board board;
    RealTimeArbiter arbiter;
    Position selected = {-1, -1};
    bool gameOver = false;

    bool isMovementLegal(std::shared_ptr<Piece> piece, const Position& from,
                          const Position& to, bool isCapture) const;
    void applyCaptureEvents(const std::vector<CaptureEvent>& events);
public:
    GameEngine() : arbiter(board) {}

    void loadBoard(const std::vector<std::vector<std::string>>& grid) { board.setGrid(grid); }
    void select(const Position& pos);
    void jump(const Position& pos);
    void wait(int ms);

    GameSnapshot snapshot() const;
};
#endif
