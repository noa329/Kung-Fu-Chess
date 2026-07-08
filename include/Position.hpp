#ifndef POSITION_H
#define POSITION_H
struct Position {
    int row, col;
    bool operator==(const Position& other) const {
        return row == other.row && col == other.col;
    }
};
#endif