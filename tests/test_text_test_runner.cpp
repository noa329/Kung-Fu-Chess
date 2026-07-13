#include "doctest.h"
#include "TextTestRunner.hpp"
#include "GameEngine.hpp"
#include <sstream>

// שכבת TextTestRunner: פירוש סקריפט פקודות והרצתו דרך נתיב הפקודות
// הציבורי (Controller/GameEngine) בלבד. אין כאן כללי תנועה, שינוי
// Board ישיר, או שכפול לוגיקת משחק.

TEST_CASE("TextTestRunner executes click/wait/print board commands") {
    GameEngine game;
    game.loadBoard({{"wR", ".", "."}});
    TextTestRunner runner(game);

    std::istringstream commands("click 50 50\nclick 250 50\nwait 2000\nprint board\n");
    std::ostringstream out;
    runner.run(commands, out);

    CHECK(out.str() == ". . wR\n");
}

TEST_CASE("TextTestRunner executes a jump command") {
    GameEngine game;
    game.loadBoard({{"wN", "."}});
    TextTestRunner runner(game);

    std::istringstream commands("jump 50 50\nprint board\n");
    std::ostringstream out;
    runner.run(commands, out);

    CHECK(out.str() == "wN .\n"); // עדיין באוויר, לא זז מהמקום
}

TEST_CASE("TextTestRunner ignores blank lines between commands") {
    GameEngine game;
    game.loadBoard({{"wK"}});
    TextTestRunner runner(game);

    std::istringstream commands("\n\nprint board\n\n");
    std::ostringstream out;
    runner.run(commands, out);

    CHECK(out.str() == "wK\n");
}
