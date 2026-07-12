#include <iostream>
#include <sstream>
#include <vector>
#include "GameEngine.hpp"
#include "Controller.hpp"
#include "BoardParser.hpp"
#include "BoardPrinter.hpp"

// TextTestRunner (מריץ בדיקות טקסט ללא UI אמיתי): פירוש הגדרת הלוח
// מואצל ל-BoardParser, הדפסתו ל-BoardPrinter, וביצוע הפקודות דרך
// ה-API הציבורי של Controller/GameEngine בלבד.

static inline std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

int main() {
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(NULL);

    auto boardResult = BoardParser::parse(std::cin);
    if (!boardResult.ok) {
        std::cout << boardResult.error << std::endl;
        return 0;
    }

    GameEngine game;
    Controller controller(game);
    game.loadBoard(boardResult.tokens);

    std::string line;
    while (std::getline(std::cin, line)) {
        line = trim(line);
        if (line.empty()) continue;
        std::stringstream ss(line);
        std::string cmd;
        ss >> cmd;
        if (cmd == "click") {
            int x, y; ss >> x >> y;
            controller.handleClick(x, y);
        } else if (cmd == "jump") {
            int x, y; ss >> x >> y;
            controller.handleJump(x, y);
        } else if (cmd == "wait") {
            int ms; ss >> ms;
            game.wait(ms);
        } else if (line == "print board") {
            BoardPrinter::print(game.snapshot().boardTokens, std::cout);
        }
    }
    return 0;
}
