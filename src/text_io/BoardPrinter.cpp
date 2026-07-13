#include "BoardPrinter.hpp"

namespace BoardPrinter {

void print(const std::vector<std::vector<std::string>>& tokens, std::ostream& out) {
    for (size_t i = 0; i < tokens.size(); ++i) {
        for (size_t j = 0; j < tokens[i].size(); ++j) {
            out << tokens[i][j] << (j == tokens[i].size() - 1 ? "" : " ");
        }
        out << "\n";
    }
}

}
