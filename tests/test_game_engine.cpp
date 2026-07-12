#include "doctest.h"
#include "../include/game_engine/GameEngine.hpp"

// שכבת GameEngine: תיאום שירות-אפליקציה בלבד - שמירה על תנאי סיום משחק,
// האצלת אימות ל-RuleEngine, התחלת תנועות חוקיות דרך RealTimeArbiter,
// האצלת wait, ותמונות מצב (snapshots). מקבלת קואורדינטות לוח (Position)
// בלבד - לא פיקסלים; מיפוי פיקסלים הוא תפקידו של Controller (שלב הבא).

TEST_CASE("selecting outside the board is ignored") {
    GameEngine engine;
    engine.loadBoard({{"wK", ".", "."}});
    engine.select({0, -1});
    engine.select({0, 3});
    auto snap = engine.snapshot();
    CHECK(snap.boardTokens == std::vector<std::vector<std::string>>{{"wK", ".", "."}});
}

TEST_CASE("selecting then moving to an empty cell schedules a move") {
    GameEngine engine;
    engine.loadBoard({{"wR", ".", "."}});
    engine.select({0, 0});
    engine.select({0, 2});
    engine.wait(2000);
    auto snap = engine.snapshot();
    CHECK(snap.boardTokens == std::vector<std::vector<std::string>>{{".", ".", "wR"}});
}

TEST_CASE("snapshot reports game-over after a king is captured") {
    GameEngine engine;
    engine.loadBoard({{"wR", "bK"}});
    CHECK(engine.snapshot().gameOver == false);
    engine.select({0, 0});
    engine.select({0, 1});
    engine.wait(1000);
    CHECK(engine.snapshot().gameOver == true);
}

TEST_CASE("snapshot reports the currently selected cell") {
    GameEngine engine;
    engine.loadBoard({{"wR", ".", "."}});
    CHECK(engine.snapshot().selected == Position{-1, -1});
    engine.select({0, 0});
    CHECK(engine.snapshot().selected == Position{0, 0});
}
