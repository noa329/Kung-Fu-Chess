#ifndef GAME_ENGINE_H
#define GAME_ENGINE_H
#include "Board.hpp"
#include "Position.hpp"
#include "RealTimeArbiter.hpp"
#include <vector>
#include <string>
#include <memory>

struct MoveRecord {
    long long atMs;
    char color;
    std::string notation;
};

struct GameSnapshot {
    std::vector<std::vector<std::string>> boardTokens;
    std::vector<std::vector<std::string>> cellStates; // "idle" | "move" | "jump", per occupied cell
    std::vector<std::vector<Position>> moveTargets;
    std::vector<std::vector<double>> moveProgress;
    Position selected;
    bool gameOver;
    std::string whiteName;
    std::string blackName;
    int whiteScore;
    int blackScore;
    std::vector<MoveRecord> whiteMoves;
    std::vector<MoveRecord> blackMoves;
};

class GameEngine {
    Board board;
    RealTimeArbiter arbiter;
    Position selected = {-1, -1};
    bool gameOver = false;
    std::string whiteName_ = "White";
    std::string blackName_ = "Black";
    int whiteScore_ = 0;
    int blackScore_ = 0;
    long long clock_ = 0;
    std::vector<MoveRecord> moveHistory_;

    bool isMovementLegal(std::shared_ptr<Piece> piece, const Position& from,
                          const Position& to, bool isCapture) const;
    void applyCaptureEvents(const std::vector<CaptureEvent>& events);
public:
    GameEngine() : arbiter(board) {}

    void loadBoard(const std::vector<std::vector<std::string>>& grid) { board.setGrid(grid); }
    void select(const Position& pos);
    void jump(const Position& pos);
    void wait(int ms);

    void setPlayerNames(const std::string& whiteName, const std::string& blackName);
    const std::string& getWhiteName() const { return whiteName_; }
    const std::string& getBlackName() const { return blackName_; }

    GameSnapshot snapshot() const;
};
#endif
