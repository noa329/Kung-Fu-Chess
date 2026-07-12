#ifndef GAME_ENGINE_H
#define GAME_ENGINE_H
#include "Board.hpp"
#include "Position.hpp"
#include "RealTimeArbiter.hpp"
#include <vector>
#include <string>
#include <memory>

// שכבת GameEngine (ראו טבלת בעלות השכבות):
// בעלות: תיאום שירות-אפליקציה - שמירה על תנאי סיום משחק, האצלת אימות
// (RuleEngine), התחלת תנועות חוקיות (RealTimeArbiter), האצלת wait,
// ותמונות מצב (snapshots).
// אסור לה: לוגיקת תנועה ספציפית לכלי, רינדור, פירוש קלט, פירוש DSL,
// או מיפוי פיקסלים.
//
// לכן ה-API כאן מקבל אך ורק קואורדינטות לוח (Position) - לא פיקסלים.
// המרת קליק-עכבר לקואורדינטת לוח היא תפקידו של Controller (השכבה הבאה).

struct GameSnapshot {
    std::vector<std::vector<std::string>> boardTokens; // "." לתא ריק, אחרת "<צבע><סוג>"
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

    // בוחר כלי אם עדיין לא נבחר; אם כבר נבחר, מנסה להתחיל מהלך/אכילה
    // אל pos (מקביל להתנהגות ה-click הישנה, בלי מיפוי פיקסלים).
    void select(const Position& pos);
    void jump(const Position& pos);
    void wait(int ms);

    GameSnapshot snapshot() const;
};
#endif
