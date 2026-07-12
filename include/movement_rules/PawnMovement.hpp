#ifndef PAWN_MOVEMENT_H
#define PAWN_MOVEMENT_H
#include "IMovementStrategy.hpp"

// לחייל, בשונה מכל שאר הכלים, כלל האכילה שונה מכלל התנועה - לכן הוא
// היחיד שדורס את isValidCapture במקום להישען על ברירת המחדל בממשק.
class PawnMovement : public IMovementStrategy {
public:
    bool isValidShape(char color, const Position& from, const Position& to, int boardRows) const override;
    bool isValidCapture(char color, const Position& from, const Position& to, int boardRows) const override;
    bool isSliding() const override { return true; } // ראו הערה ב-MovementRules המקורי: תומך בבדיקת מסלול לצעד הכפול
};
#endif
