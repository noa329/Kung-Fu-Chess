#include "doctest.h"
#include "../include/renderer/Renderer.hpp"
#include <sstream>

// שכבת Renderer: ציור מ-GameSnapshot בלבד, לקריאה. אין כאן כללי משחק,
// שינוי Board, פירוש קלט, או לוגיקת בדיקות-טקסט.
//
// הערה: בפרויקט הזה אין "ספריית ציור" גרפית מסופקת - Renderer זה
// מייצר ייצוג טקסטואלי-חזותי (עם קואורדינטות) לקונסולה, כתחליף דק
// וניתן-להחלפה. אם בעתיד תסופק ספריית ציור אמיתית, רק המימוש הפנימי
// של render() ישתנה - שום שכבה אחרת לא תדע על זה.

TEST_CASE("Renderer draws board tokens with row/col coordinates") {
    GameSnapshot snap;
    snap.boardTokens = {{"wK", "."}, {".", "bQ"}};
    snap.selected = {-1, -1};
    snap.gameOver = false;

    std::ostringstream out;
    Renderer renderer;
    renderer.render(snap, out);

    std::string result = out.str();
    CHECK(result.find("wK") != std::string::npos);
    CHECK(result.find("bQ") != std::string::npos);
}

TEST_CASE("Renderer marks the selected cell") {
    GameSnapshot snap;
    snap.boardTokens = {{"wR", "."}};
    snap.selected = {0, 0};
    snap.gameOver = false;

    std::ostringstream out;
    Renderer renderer;
    renderer.render(snap, out);

    CHECK(out.str().find("[wR]") != std::string::npos);
}

TEST_CASE("Renderer reports game over") {
    GameSnapshot snap;
    snap.boardTokens = {{"."}};
    snap.selected = {-1, -1};
    snap.gameOver = true;

    std::ostringstream out;
    Renderer renderer;
    renderer.render(snap, out);

    CHECK(out.str().find("GAME OVER") != std::string::npos);
}
