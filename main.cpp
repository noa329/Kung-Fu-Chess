#include <iostream>
#include "GameEngine.hpp"
#include "BoardParser.hpp"
#include "TextTestRunner.hpp"

int main() {
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(NULL);

    auto boardResult = BoardParser::parse(std::cin);
    if (!boardResult.ok) {
        std::cout << boardResult.error << std::endl;
        return 0;
    }

    GameEngine game;
    game.startGame(boardResult.tokens);

    TextTestRunner runner(game);
    runner.run(std::cin, std::cout);
    return 0;
}
