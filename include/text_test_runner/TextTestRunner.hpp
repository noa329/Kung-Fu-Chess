#ifndef TEXT_TEST_RUNNER_H
#define TEXT_TEST_RUNNER_H
#include <istream>
#include <ostream>
#include "GameEngine.hpp"
#include "Controller.hpp"

class TextTestRunner {
    GameEngine& engine;
    Controller controller;
public:
    explicit TextTestRunner(GameEngine& e) : engine(e), controller(e) {}
    void run(std::istream& commands, std::ostream& out);
};
#endif
