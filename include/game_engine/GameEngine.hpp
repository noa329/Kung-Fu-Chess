#ifndef GAME_ENGINE_H
#define GAME_ENGINE_H
#include "Board.hpp"
#include "Position.hpp"
#include "RealTimeArbiter.hpp"
#include "EventBus.hpp"
#include <vector>
#include <string>
#include <memory>

struct MoveRecord {
    long long atMs;
    char color;
    std::string notation;
};

struct ActiveCapture {
    Position at;
    char capturedColor;
    bool wasKing;
    long long startTime;
    long long endTime;
};

struct CaptureFlash {
    Position at;
    char capturedColor;
    bool wasKing;
    double progress; // 0.0 (just happened) .. 1.0 (about to disappear)
};

struct GameSnapshot {
    std::vector<std::vector<std::string>> boardTokens;
    std::vector<std::vector<std::string>> cellStates; // "idle" | "move" | "jump" | "short_rest" | "long_rest", per occupied cell
    std::vector<std::vector<Position>> moveTargets;
    std::vector<std::vector<double>> moveProgress;
    Position selected;
    bool gameOver;
    std::string result; // "White Wins" | "Black Wins" | "Draw", meaningful only when gameOver
    std::string whiteName;
    std::string blackName;
    int whiteScore;
    int blackScore;
    std::vector<MoveRecord> whiteMoves;
    std::vector<MoveRecord> blackMoves;
    std::vector<CaptureFlash> captureFlashes;
};

class GameEngine {
    Board board;
    RealTimeArbiter arbiter;
    Position selected = {-1, -1};
    bool gameOver = false;
    char winnerColor_ = '\0'; // 'w' | 'b' | '\0' (no winner yet, or draw)
    std::string whiteName_ = "White";
    std::string blackName_ = "Black";
    int whiteScore_ = 0;
    int blackScore_ = 0;
    long long clock_ = 0;
    std::vector<MoveRecord> moveHistory_;
    std::vector<ActiveCapture> activeCaptures_;
    EventBus events_;

    static const long long CAPTURE_EFFECT_MS = 400;

    bool isMovementLegal(std::shared_ptr<Piece> piece, const Position& from,
                          const Position& to, bool isCapture) const;
    void applyCaptureEvents(const std::vector<CaptureEvent>& events);
    void pruneCaptureFlashes();
public:
    GameEngine() : arbiter(board) {}

    void loadBoard(const std::vector<std::vector<std::string>>& grid) { board.setGrid(grid); }
    // Wraps loadBoard() and fires onGameLifecycle({"start", ""}) - the entry
    // point composition roots (main.cpp) should call to begin a real game
    // session, as opposed to loadBoard() alone (used freely by tests, which
    // don't care about the lifecycle event).
    void startGame(const std::vector<std::vector<std::string>>& grid);
    // Task E3: fires onGameLifecycle({"start", ""}) alone, without
    // touching the board - for a caller that already loadBoard()'d
    // separately and wants to defer just the *announcement* of game
    // start. Room sessions need this: the board is loaded (and broadcast)
    // the moment a room is created so its lone creator sees the starting
    // position while waiting, but the "game start" lifecycle event (and
    // therefore its log line, and any future ELO/analytics hook) should
    // only fire once a real 2nd player has actually joined - same
    // "exactly 2 known participants" moment matchmaking's startGame()
    // call already gets for free by only ever creating a session once
    // both players are matched.
    void announceStart();
    void select(const Position& pos);
    void jump(const Position& pos);
    void wait(int ms);

    // Each GameEngine owns its own EventBus, so every game session gets an
    // isolated event stream (no cross-talk between concurrent games once the
    // server layer exists). Subscribers (SoundManager glue, UI hooks) call
    // engine.events().onX.subscribe(...) from the composition root.
    EventBus& events() { return events_; }

    // Ends the game immediately in favor of the other color, same
    // gameOver/winnerColor_/onGameLifecycle("end", ...) path a king capture
    // already drives (applyCaptureEvents) - so GameSnapshot::gameOver stays
    // the one source of truth regardless of *why* the game ended. Used by
    // the server's disconnect-timeout auto-resign (Task D4); a no-op if the
    // game already ended (mirrors select()/jump()'s own `if (gameOver)
    // return;` guard).
    void resign(char color);

    void setPlayerNames(const std::string& whiteName, const std::string& blackName);
    void setRestDurations(long long longRestMs, long long shortRestMs);
    const std::string& getWhiteName() const { return whiteName_; }
    const std::string& getBlackName() const { return blackName_; }

    GameSnapshot snapshot() const;
};
#endif
