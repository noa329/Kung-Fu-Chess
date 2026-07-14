#ifndef CONTROLLER_H
#define CONTROLLER_H
#include "GameEngine.hpp"
#include "Position.hpp"

class Controller {
    GameEngine& engine;
    int cellPixelSize;

    Position pixelToGrid(int x, int y) const;
public:
    explicit Controller(GameEngine& e, int cellPixelSize = 100)
        : engine(e), cellPixelSize(cellPixelSize) {}

    void handleClick(int x, int y);
    void handleJump(int x, int y);
};
#endif
