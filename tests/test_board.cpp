#include "doctest.h"
#include "../include/Board.h"

TEST_CASE("pixelToGrid converts pixels to cells correctly") {
    Board board;
    CHECK(board.pixelToGrid(50, 50).row == 0);
    CHECK(board.pixelToGrid(50, 50).col == 0);
    CHECK(board.pixelToGrid(250, 150).row == 1);
    CHECK(board.pixelToGrid(250, 150).col == 2);
}

TEST_CASE("isInside detects out-of-bounds correctly") {
    std::vector<std::vector<std::string>> grid = {{".", "."}, {".", "."}};
    Board board;
    board.setGrid(grid);
    CHECK(board.isInside(0, 0) == true);
    CHECK(board.isInside(5, 5) == false);
    CHECK(board.isInside(-1, 0) == false);
}