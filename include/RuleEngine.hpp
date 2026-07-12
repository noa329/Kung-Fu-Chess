#ifndef RULE_ENGINE_H
#define RULE_ENGINE_H
#include <memory>
#include "Board.hpp"
#include "Piece.hpp"
#include "Position.hpp"

// שכבת RuleEngine (ראו טבלת בעלות השכבות):
// בעלות: אימות חוקיות מהלך מבוקש, קריאה בלבד.
// אסור לה: שינוי Board, אנימציה, פירוש קליקים, או מצב סיום משחק.
//
// RuleEngine מחזיק רפרנס קבוע (const) ל-Board כדי לשאול שאלות תפוסה
// (isPathClear) ומאציל את הגיאומטריה עצמה ל-MovementRules. הוא לא יודע
// כלום על תנועות ממתינות/זמן-אמת - זה תפקידה של RealTimeArbiter.
class RuleEngine {
    const Board& board;
public:
    explicit RuleEngine(const Board& b) : board(b) {}

    bool isLegal(const std::shared_ptr<Piece>& piece, const Position& from,
                 const Position& to, bool isCapture) const;
};
#endif
