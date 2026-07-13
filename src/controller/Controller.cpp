#include "Controller.hpp"

Position Controller::pixelToGrid(int x, int y) const {
    int r = (y >= 0) ? y / cellPixelSize : (y - (cellPixelSize - 1)) / cellPixelSize;
    int c = (x >= 0) ? x / cellPixelSize : (x - (cellPixelSize - 1)) / cellPixelSize;
    return {r, c};
}

void Controller::handleClick(int x, int y) {
    engine.select(pixelToGrid(x, y));
}

void Controller::handleJump(int x, int y) {
    engine.jump(pixelToGrid(x, y));
}
