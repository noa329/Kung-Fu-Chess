#include "doctest.h"
#include "GameEngine.hpp"

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

// דרישת חובה מהמנחה: אי אפשר לצוות על כלי ללכת למקום חדש כאשר הוא
// כבר באמצע הליכה (mid-flight). מוודאים כאן שניסיון שני "מבטל" את
// הראשון לא משנה את יעד ההגעה בפועל.
TEST_CASE("a piece already mid-move cannot be redirected to a new destination") {
    GameEngine engine;
    engine.loadBoard({{"wR", ".", ".", ".", "."}});
    engine.select({0, 0});
    engine.select({0, 4}); // מהלך ארוך, לא יגיע לפני ה-wait הראשון

    // ניסיון "לחטוף" את הכלי באמצע הדרך ולשלוח אותו למקום אחר -
    // חייב להיות מתעלם לגמרי, כי מדובר בתא-המקור של מהלך ממתין.
    engine.select({0, 0});
    engine.select({0, 1});

    engine.wait(4000); // מספיק זמן להגעה למהלך המקורי (מרחק 4 -> 4000ms)
    auto snap = engine.snapshot();
    CHECK(snap.boardTokens == std::vector<std::vector<std::string>>{{".", ".", ".", ".", "wR"}});
}
