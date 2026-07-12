#ifndef MOVEMENT_STRATEGY_FACTORY_H
#define MOVEMENT_STRATEGY_FACTORY_H
#include "IMovementStrategy.hpp"
#include "model/PieceKind.hpp"

// מחזיר את אסטרטגיית התנועה המתאימה ל-PieceKind. הרפרנס המוחזר הוא
// לאובייקט סטטי חסר-מצב (stateless) - אין הקצאה בכל קריאה.
const IMovementStrategy& getMovementStrategy(PieceKind kind);
#endif
