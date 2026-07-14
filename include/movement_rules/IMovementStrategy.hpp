#ifndef I_MOVEMENT_STRATEGY_H
#define I_MOVEMENT_STRATEGY_H
#include "Position.hpp"

class IMovementStrategy {
public:
    virtual ~IMovementStrategy() = default;
    virtual bool isValidShape(char color, const Position& from, const Position& to, int boardRows) const = 0;

    // ברירת מחדל: זהה ל-isValidShape. כלים עם כלל אכילה שונה (חייל) דורסים אותה.
    virtual bool isValidCapture(char color, const Position& from, const Position& to, int boardRows) const {
        return isValidShape(color, from, to, boardRows);
    }

    virtual bool isSliding() const = 0;
};
#endif
