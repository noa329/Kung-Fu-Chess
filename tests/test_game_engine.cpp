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

// המהות של "קונג פו שחמט": שני הצדדים זזים בזמן-אמת בלי תורות. כלי של
// צבע אחד לא אמור לחכות שכלי מהצבע השני יסיים לנוע לפני שהוא יכול
// להתחיל לזוז בעצמו.
TEST_CASE("pieces of opposite colors can move concurrently") {
    GameEngine engine;
    engine.loadBoard({{"wR", ".", ".", ".", "."}, {"bR", ".", ".", ".", "."}});

    engine.select({0, 0});
    engine.select({0, 4}); // מהלך לבן, מרחק 4 -> 4000ms

    engine.select({1, 0});
    engine.select({1, 2}); // מהלך שחור, מרחק 2 -> 2000ms - מתוזמן בזמן שהלבן עדיין באוויר

    engine.wait(4000);
    auto snap = engine.snapshot();
    CHECK(snap.boardTokens == std::vector<std::vector<std::string>>{
        {".", ".", ".", ".", "wR"},
        {".", ".", "bR", ".", "."}
    });
}

// שלב 8: שמות שחקנים, ניקוד ("cost" של כלים שנאכלו), ולוג מהלכים -
// הרחבות ל-GameSnapshot בלבד, אין שינוי לחוקיות מהלכים.

TEST_CASE("player names default to White/Black and can be overridden") {
    GameEngine engine;
    engine.loadBoard({{"wK"}});
    CHECK(engine.getWhiteName() == "White");
    CHECK(engine.getBlackName() == "Black");
    engine.setPlayerNames("Alice", "Bob");
    CHECK(engine.getWhiteName() == "Alice");
    CHECK(engine.getBlackName() == "Bob");
    auto snap = engine.snapshot();
    CHECK(snap.whiteName == "Alice");
    CHECK(snap.blackName == "Bob");
}

TEST_CASE("capturing a piece credits the capturing side with its value") {
    GameEngine engine;
    engine.loadBoard({{"wR", "bN"}});
    CHECK(engine.snapshot().whiteScore == 0);
    CHECK(engine.snapshot().blackScore == 0);
    engine.select({0, 0});
    engine.select({0, 1}); // white rook captures black knight (value 3)
    engine.wait(1000);
    auto snap = engine.snapshot();
    CHECK(snap.whiteScore == 3);
    CHECK(snap.blackScore == 0);
}

TEST_CASE("capturing a king ends the game without changing the score") {
    GameEngine engine;
    engine.loadBoard({{"wR", "bK"}});
    engine.select({0, 0});
    engine.select({0, 1});
    engine.wait(1000);
    auto snap = engine.snapshot();
    CHECK(snap.gameOver == true);
    CHECK(snap.whiteScore == 0);
}

TEST_CASE("a move is appended to the history when scheduled, not when it lands") {
    GameEngine engine;
    engine.loadBoard({{"wR", ".", ".", ".", "."}});
    engine.select({0, 0});
    engine.select({0, 4}); // long move, won't land immediately

    auto snapBeforeArrival = engine.snapshot();
    REQUIRE(snapBeforeArrival.whiteMoves.size() == 1);
    CHECK(snapBeforeArrival.whiteMoves[0].color == 'w');
    CHECK(snapBeforeArrival.whiteMoves[0].notation == "a1e1"); // 1-row board: rank = rowCount(1) - row(0) = 1
    CHECK(snapBeforeArrival.blackMoves.empty());

    engine.wait(4000);
    auto snapAfterArrival = engine.snapshot();
    CHECK(snapAfterArrival.whiteMoves.size() == 1); // still just the one scheduling, not a second entry on arrival
}

TEST_CASE("a jump is appended to the history too") {
    GameEngine engine;
    engine.loadBoard({{"wN"}});
    engine.jump({0, 0});
    auto snap = engine.snapshot();
    REQUIRE(snap.whiteMoves.size() == 1);
    CHECK(snap.whiteMoves[0].color == 'w');
    CHECK(snap.whiteMoves[0].notation == "a1");
}

// שלב 3: אכיפת מנוחה (cooldown) - כלי שהגיע ליעד (long_rest) או שסיים
// קפיצה (short_rest) לא יכול לזוז/לקפוץ שוב עד שהמנוחה מסתיימת.

TEST_CASE("a piece in long rest after landing cannot be moved again until the cooldown expires") {
    GameEngine engine;
    engine.loadBoard({{"wR", ".", ".", ".", "."}});
    engine.select({0, 0});
    engine.select({0, 2}); // distance 2 -> 2000ms
    engine.wait(2000); // lands at (0,2), enters long rest

    CHECK(engine.snapshot().cellStates[0][2] == "long_rest");

    // attempt to move it again immediately - must be silently rejected
    engine.select({0, 2});
    engine.select({0, 4});
    CHECK(engine.snapshot().boardTokens == std::vector<std::vector<std::string>>{{".", ".", "wR", ".", "."}});

    engine.wait(799); // 799ms into the 800ms long rest
    CHECK(engine.snapshot().cellStates[0][2] == "long_rest");

    engine.wait(1); // rest expires exactly now
    CHECK(engine.snapshot().cellStates[0][2] == "idle");

    // the same move now succeeds
    engine.select({0, 2});
    engine.select({0, 4});
    engine.wait(2000);
    CHECK(engine.snapshot().boardTokens == std::vector<std::vector<std::string>>{{".", ".", ".", ".", "wR"}});
}

TEST_CASE("a piece in short rest after a jump cannot jump again until the cooldown expires") {
    GameEngine engine;
    engine.loadBoard({{"wN", "."}});
    engine.jump({0, 0});
    engine.wait(1000); // jump duration elapses, piece lands in short rest

    CHECK(engine.snapshot().cellStates[0][0] == "short_rest");

    engine.jump({0, 0}); // attempt again immediately - must be silently rejected
    CHECK(engine.snapshot().whiteMoves.size() == 1); // no new history entry

    engine.wait(499); // 499ms into the 500ms short rest
    CHECK(engine.snapshot().cellStates[0][0] == "short_rest");

    engine.wait(1); // rest expires exactly now
    CHECK(engine.snapshot().cellStates[0][0] == "idle");

    engine.jump({0, 0}); // now succeeds
    CHECK(engine.snapshot().whiteMoves.size() == 2);
}

// שלב 7: משכי מנוחה מבוססי-קונפיג - GameEngine::setRestDurations מעביר את
// הערכים ל-RealTimeArbiter; אם אף אחד לא קורא לזה, ברירת המחדל (800/500)
// ממשיכה לעבוד בדיוק כמו קודם.

TEST_CASE("GameEngine::setRestDurations overrides how long a landed move's long_rest lasts") {
    GameEngine engine;
    engine.loadBoard({{"wR", ".", "."}});
    engine.setRestDurations(/*longRestMs=*/300, /*shortRestMs=*/150);
    engine.select({0, 0});
    engine.select({0, 2}); // distance 2 -> 2000ms
    engine.wait(2000); // lands, long_rest now lasts 300ms instead of 800ms

    CHECK(engine.snapshot().cellStates[0][2] == "long_rest");
    engine.wait(299);
    CHECK(engine.snapshot().cellStates[0][2] == "long_rest");
    engine.wait(1);
    CHECK(engine.snapshot().cellStates[0][2] == "idle");
}

// שלב 4: הבזק תפיסה (capture flash) - אפקט חזותי בלבד, לא משפיע על חוקיות
// או על boardTokens; דועך במשך CAPTURE_EFFECT_MS ואז נעלם מה-snapshot.

TEST_CASE("a capture creates a decaying capture flash that disappears after its effect duration") {
    GameEngine engine;
    engine.loadBoard({{"wR", "bN"}});
    engine.select({0, 0});
    engine.select({0, 1}); // white rook captures black knight, capture duration 1000ms
    engine.wait(1000); // the capture lands now

    auto snap = engine.snapshot();
    REQUIRE(snap.captureFlashes.size() == 1);
    CHECK(snap.captureFlashes[0].at == Position{0, 1});
    CHECK(snap.captureFlashes[0].capturedColor == 'b');
    CHECK(snap.captureFlashes[0].wasKing == false);
    CHECK(snap.captureFlashes[0].progress == doctest::Approx(0.0));
    CHECK(snap.boardTokens == std::vector<std::vector<std::string>>{{".", "wR"}}); // purely decorative

    engine.wait(200); // halfway through the 400ms effect
    CHECK(engine.snapshot().captureFlashes[0].progress == doctest::Approx(0.5));

    engine.wait(199); // 399ms total - still active
    CHECK(engine.snapshot().captureFlashes.size() == 1);

    engine.wait(1); // 400ms total - effect expires
    CHECK(engine.snapshot().captureFlashes.empty());
}

TEST_CASE("a captured king's flash is reported with wasKing set") {
    GameEngine engine;
    engine.loadBoard({{"wR", "bK"}});
    engine.select({0, 0});
    engine.select({0, 1});
    engine.wait(1000);
    auto snap = engine.snapshot();
    REQUIRE(snap.captureFlashes.size() == 1);
    CHECK(snap.captureFlashes[0].wasKing == true);
}
