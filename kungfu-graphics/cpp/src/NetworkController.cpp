#include "NetworkController.hpp"

// Identical formula to Controller::pixelToGrid (include/controller/
// Controller.cpp) - both convert the same pixel space to the same board
// grid, just for two different downstream actions (GameEngine::select()/
// jump() locally, a wire command here).
Position NetworkController::pixelToGrid(int x, int y) const {
    int r = (y >= 0) ? y / cellPixelSize_ : (y - (cellPixelSize_ - 1)) / cellPixelSize_;
    int c = (x >= 0) ? x / cellPixelSize_ : (x - (cellPixelSize_ - 1)) / cellPixelSize_;
    return {r, c};
}

void NetworkController::handleClick(int x, int y) {
    if (!snapshot_) return;
    if (auto command = handler_.handleClick(*snapshot_, pixelToGrid(x, y))) {
        client_.sendCommand(*command);
    }
}

void NetworkController::handleJump(int x, int y) {
    if (!snapshot_) return;
    if (auto command = handler_.handleJump(*snapshot_, pixelToGrid(x, y))) {
        client_.sendCommand(*command);
    }
}
