#ifndef GAME_H
#define GAME_H
#include "Board.h"

class Game {
    Board board;
    Position selected = {-1, -1};
    long long currentTime = 0;
public:
    void loadBoard(const std::vector<std::vector<std::string>>& grid) { board.setGrid(grid); }
    void click(int x, int y);
    void wait(int ms) { currentTime += ms; }
    void printBoard() const { board.print(); }
};
#endif