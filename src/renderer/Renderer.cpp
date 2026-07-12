#include "renderer/Renderer.hpp"

void Renderer::render(const GameSnapshot& snapshot, std::ostream& out) const {
    out << "   ";
    for (size_t c = 0; c < (snapshot.boardTokens.empty() ? 0 : snapshot.boardTokens[0].size()); ++c) {
        out << " " << c << "  ";
    }
    out << "\n";

    for (size_t r = 0; r < snapshot.boardTokens.size(); ++r) {
        out << r << " |";
        for (size_t c = 0; c < snapshot.boardTokens[r].size(); ++c) {
            const std::string& token = snapshot.boardTokens[r][c];
            bool isSelected = snapshot.selected.row == (int)r && snapshot.selected.col == (int)c;
            out << (isSelected ? "[" : " ") << token << (isSelected ? "]" : " ") << "|";
        }
        out << "\n";
    }

    if (snapshot.gameOver) {
        out << "GAME OVER\n";
    }
}
