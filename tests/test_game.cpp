// tests/test_game.cpp
#include "doctest.h"
#include "../include/Game.hpp"
#include <sstream>
#include <iostream>
#include <sstream>

// טריק שימושי: לוכדים את std::cout כדי לבדוק פלט של printBoard()
std::string captureBoard(Game &game)
{
    std::stringstream buffer;
    std::streambuf *old = std::cout.rdbuf(buffer.rdbuf());
    game.printBoard();
    std::cout.rdbuf(old);
    return buffer.str();
}

TEST_CASE("clicking outside board is ignored")
{
    Game game;
    game.loadBoard({{"wK", ".", "."}});
    game.click(-10, 50);
    game.click(350, 50);
    CHECK(captureBoard(game) == "wK . .\n");
}

TEST_CASE("selecting then moving to empty cell schedules a move")
{
    Game game;
    game.loadBoard({{"wR", ".", "."}});
    game.click(50, 50);
    game.click(250, 50);
    game.wait(2000);
    CHECK(captureBoard(game) == ". . wR\n");
}