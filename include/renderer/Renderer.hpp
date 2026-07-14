#ifndef RENDERER_H
#define RENDERER_H
#include <ostream>
#include "GameEngine.hpp"

class Renderer {
public:
    void render(const GameSnapshot& snapshot, std::ostream& out) const;
};
#endif
