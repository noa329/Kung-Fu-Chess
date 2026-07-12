#include <iostream>
#include "game_engine/GameEngine.hpp"
#include "text_io/BoardParser.hpp"
#include "text_test_runner/TextTestRunner.hpp"

int main() {
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(NULL);

    auto boardResult = BoardParser::parse(std::cin);
    if (!boardResult.ok) {
        std::cout << boardResult.error << std::endl;
        return 0;
    }

    GameEngine game;
    game.loadBoard(boardResult.tokens);

    TextTestRunner runner(game);
    runner.run(std::cin, std::cout);
    return 0;
}
