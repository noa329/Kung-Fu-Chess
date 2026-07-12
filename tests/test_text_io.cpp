#include "doctest.h"
#include "../include/text_io/BoardParser.hpp"
#include "../include/text_io/BoardPrinter.hpp"
#include <sstream>

// שכבת Text I/O: BoardParser (פירוש הגדרת לוח טקסטואלית) ו-BoardPrinter
// (הדפסת מצב לוח לוגי). אין כאן כללי תנועה, ביצוע פקודות, רינדור, או
// לוגיקת בדיקות מעבר להשוואת טקסט.

TEST_CASE("BoardParser reads rows until the Commands: marker") {
    std::istringstream in("Board:\nwK . .\n. . bK\nCommands:\nclick 1 1\n");
    auto result = BoardParser::parse(in);
    CHECK(result.ok == true);
    CHECK(result.tokens == std::vector<std::vector<std::string>>{{"wK", ".", "."}, {".", ".", "bK"}});
}

TEST_CASE("BoardParser rejects an unknown token") {
    std::istringstream in("Board:\nwX . .\nCommands:\n");
    auto result = BoardParser::parse(in);
    CHECK(result.ok == false);
    CHECK(result.error == "ERROR UNKNOWN_TOKEN");
}

TEST_CASE("BoardParser rejects mismatched row widths") {
    std::istringstream in("Board:\nwK . .\n. .\nCommands:\n");
    auto result = BoardParser::parse(in);
    CHECK(result.ok == false);
    CHECK(result.error == "ERROR ROW_WIDTH_MISMATCH");
}

TEST_CASE("BoardPrinter prints tokens exactly like the old Board::print format") {
    std::vector<std::vector<std::string>> tokens = {{"wK", ".", "."}, {".", ".", "bK"}};
    std::ostringstream out;
    BoardPrinter::print(tokens, out);
    CHECK(out.str() == "wK . .\n. . bK\n");
}
