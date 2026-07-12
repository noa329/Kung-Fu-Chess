#include "doctest.h"
#include "../include/Controller.hpp"
#include "../include/GameEngine.hpp"

// שכבת Controller: פירוש קליקים (פיקסל -> Position) ותרגומם לפקודות
// ל-GameEngine. אין כאן חוקיות שחמט, שינוי Board ישיר, רינדור, או תזמון.

TEST_CASE("click outside the board is ignored end to end") {
    GameEngine engine;
    engine.loadBoard({{"wK", ".", "."}});
    Controller controller(engine);
    controller.handleClick(-10, 50);
    controller.handleClick(350, 50);
    auto snap = engine.snapshot();
    CHECK(snap.boardTokens == std::vector<std::vector<std::string>>{{"wK", ".", "."}});
}

TEST_CASE("two clicks select then schedule a move, mirroring the old pixel API") {
    GameEngine engine;
    engine.loadBoard({{"wR", ".", "."}});
    Controller controller(engine);
    controller.handleClick(50, 50);
    controller.handleClick(250, 50);
    engine.wait(2000);
    auto snap = engine.snapshot();
    CHECK(snap.boardTokens == std::vector<std::vector<std::string>>{{".", ".", "wR"}});
}

TEST_CASE("handleJump translates pixels to a grid jump") {
    GameEngine engine;
    engine.loadBoard({{"wN", "."}});
    Controller controller(engine);
    controller.handleJump(50, 50);
    CHECK(engine.snapshot().selected == Position{-1, -1}); // jump doesn't select
}

TEST_CASE("Controller respects a custom cell pixel size") {
    GameEngine engine;
    engine.loadBoard({{"wR", ".", "."}});
    Controller controller(engine, /*cellPixelSize=*/50);
    controller.handleClick(25, 25);   // (0,0) at 50px cells
    controller.handleClick(125, 25);  // (0,2) at 50px cells
    engine.wait(2000);
    auto snap = engine.snapshot();
    CHECK(snap.boardTokens == std::vector<std::vector<std::string>>{{".", ".", "wR"}});
}
