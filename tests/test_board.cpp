#include "doctest.h"
#include "../include/model/Board.hpp"

TEST_CASE("isInside detects out-of-bounds correctly") {
    std::vector<std::vector<std::string>> grid = {{".", "."}, {".", "."}};
    Board board;
    board.setGrid(grid);
    CHECK(board.isInside(0, 0) == true);
    CHECK(board.isInside(5, 5) == false);
    CHECK(board.isInside(-1, 0) == false);
}

TEST_CASE("getColCount reports the board width") {
    Board board;
    board.setGrid({{".", ".", "."}, {".", ".", "."}});
    CHECK(board.getColCount() == 3);
    CHECK(board.getRowCount() == 2);
}