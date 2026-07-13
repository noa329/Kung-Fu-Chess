#include "TextTestRunner.hpp"
#include "BoardPrinter.hpp"
#include <sstream>

namespace {
    std::string trim(const std::string& s) {
        size_t start = s.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) return "";
        size_t end = s.find_last_not_of(" \t\r\n");
        return s.substr(start, end - start + 1);
    }
}

void TextTestRunner::run(std::istream& commands, std::ostream& out) {
    std::string line;
    while (std::getline(commands, line)) {
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
            engine.wait(ms);
        } else if (line == "print board") {
            BoardPrinter::print(engine.snapshot().boardTokens, out);
        }
    }
}
