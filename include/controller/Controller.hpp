#ifndef CONTROLLER_H
#define CONTROLLER_H
#include "GameEngine.hpp"
#include "Position.hpp"

// שכבת Controller (ראו טבלת בעלות השכבות):
// בעלות: פירוש קליקים ומצב תא נבחר.
// אסור לה: חוקיות שחמט, שינוי Board, רינדור, או תזמון.
//
// זו הנקודה היחידה בפרויקט שממירה פיקסלים לקואורדינטת לוח - אחרי
// שהוצאנו את pixelToGrid מ-Board (שלב 1) ומ-GameEngine (שלב 4).
// "מצב תא נבחר" לצורך תצוגה זמין דרך GameEngine::snapshot().selected -
// ה-Controller לא מחזיק עותק כפול של אותו state, הוא רק מתרגם קליקים.
class Controller {
    GameEngine& engine;
    int cellPixelSize;

    Position pixelToGrid(int x, int y) const;
public:
    explicit Controller(GameEngine& e, int cellPixelSize = 100)
        : engine(e), cellPixelSize(cellPixelSize) {}

    void handleClick(int x, int y);
    void handleJump(int x, int y);
};
#endif
