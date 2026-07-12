#ifndef TEXT_TEST_RUNNER_H
#define TEXT_TEST_RUNNER_H
#include <istream>
#include <ostream>
#include "GameEngine.hpp"
#include "Controller.hpp"

// שכבת TextTestRunner (ראו טבלת בעלות השכבות):
// בעלות: פירוש סקריפט והפעלת נתיב הפקודות הציבורי.
// אסור לה: כללי תנועה, שינוי ישיר של Board, או שכפול לוגיקת משחק.
//
// מריצה תסריט פקודות טקסטואלי (click/jump/wait/print board) דרך
// ה-API הציבורי של Controller ו-GameEngine בלבד - בלי UI אמיתי.
class TextTestRunner {
    GameEngine& engine;
    Controller controller;
public:
    explicit TextTestRunner(GameEngine& e) : engine(e), controller(e) {}
    void run(std::istream& commands, std::ostream& out);
};
#endif
