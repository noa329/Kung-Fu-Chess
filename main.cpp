#include <iostream>
#include <sstream>
#include <vector>
#include "GameEngine.hpp"

static inline std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

bool isValidToken(const std::string& token) {
    if (token == ".") return true;
    if (token.length() != 2) return false;
    return (token[0] == 'w' || token[0] == 'b') &&
           (token[1] == 'K' || token[1] == 'Q' || token[1] == 'R' || token[1] == 'B' || token[1] == 'N' || token[1] == 'P');
}

// זמני (עד שלב 5 - Controller): GameEngine כבר לא מכיר פיקסלים בכלל
// (זו הייתה המטרה של שלב 4). מיפוי פיקסלים->לוח גר כאן, בנקודת הכניסה,
// ולא בתוך שום שכבת ליבה - עד שיהיה Controller אמיתי שיאמץ אותו.
static Position pixelToGrid(int x, int y, int cellSize = 100) {
    int r = (y >= 0) ? y / cellSize : (y - (cellSize - 1)) / cellSize;
    int c = (x >= 0) ? x / cellSize : (x - (cellSize - 1)) / cellSize;
    return {r, c};
}

int main() {
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(NULL);
    GameEngine game;
    std::string line;
    std::vector<std::vector<std::string>> boardData;
    size_t cols = 0;

    while (std::getline(std::cin, line)) {
        line = trim(line);
        if (line == "Commands:") break;
        if (line == "Board:" || line.empty()) continue;
        std::stringstream ss(line);
        std::string token;
        std::vector<std::string> row;
        while (ss >> token) {
            if (!isValidToken(token)) {
                std::cout << "ERROR UNKNOWN_TOKEN" << std::endl;
                return 0;
            }
            row.push_back(token);
        }
        if (row.empty()) continue;
        if (cols == 0) cols = row.size();
        else if (row.size() != cols) {
            std::cout << "ERROR ROW_WIDTH_MISMATCH" << std::endl;
            return 0;
        }
        boardData.push_back(row);
    }
    game.loadBoard(boardData);

    while (std::getline(std::cin, line)) {
        line = trim(line);
        if (line.empty()) continue;
        std::stringstream ss(line);
        std::string cmd;
        ss >> cmd;
        if (cmd == "click") {
            int x, y; ss >> x >> y;
            game.select(pixelToGrid(x, y));
        } else if (cmd == "jump") {
            int x, y; ss >> x >> y;
            game.jump(pixelToGrid(x, y));
        } else if (cmd == "wait") {
            int ms; ss >> ms;
            game.wait(ms);
        } else if (line == "print board") {
            game.printBoard();
        }
    }
    return 0;
}
