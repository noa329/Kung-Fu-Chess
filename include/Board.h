#ifndef BOARD_H
#define BOARD_H

#include <iostream>
#include <vector>
#include <string>

struct Position { int row, col; };

class Board {
    std::vector<std::vector<std::string>> grid;
public:
    void setGrid(const std::vector<std::vector<std::string>>& newGrid) { grid = newGrid; }
    
    Position pixelToGrid(int x, int y) const {
    int r = (y >= 0) ? y / 100 : (y - 99) / 100;
    int c = (x >= 0) ? x / 100 : (x - 99) / 100;
    return {r, c};
}

    bool isInside(int r, int c) const {
        return r >= 0 && r < (int)grid.size() && c >= 0 && c < (int)grid[0].size();
    }

    std::string getCell(int r, int c) const { return grid[r][c]; }
    void setCell(int r, int c, std::string val) { grid[r][c] = val; }
    
    void print() const {
        for (size_t i = 0; i < grid.size(); ++i) {
            for (size_t j = 0; j < grid[i].size(); ++j) {
                std::cout << grid[i][j] << (j == grid[i].size() - 1 ? "" : " ");
            }
            std::cout << std::endl;
        }
    }
};
#endif