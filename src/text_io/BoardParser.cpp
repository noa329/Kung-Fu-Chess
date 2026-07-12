#include "text_io/BoardParser.hpp"
#include <sstream>

namespace {
    std::string trim(const std::string& s) {
        size_t start = s.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) return "";
        size_t end = s.find_last_not_of(" \t\r\n");
        return s.substr(start, end - start + 1);
    }
}

namespace BoardParser {

bool isValidToken(const std::string& token) {
    if (token == ".") return true;
    if (token.length() != 2) return false;
    return (token[0] == 'w' || token[0] == 'b') &&
           (token[1] == 'K' || token[1] == 'Q' || token[1] == 'R' || token[1] == 'B' || token[1] == 'N' || token[1] == 'P');
}

BoardParseResult parse(std::istream& in) {
    BoardParseResult result;
    result.ok = true;
    std::string line;
    size_t cols = 0;

    while (std::getline(in, line)) {
        line = trim(line);
        if (line == "Commands:") break;
        if (line == "Board:" || line.empty()) continue;

        std::stringstream ss(line);
        std::string token;
        std::vector<std::string> row;
        while (ss >> token) {
            if (!isValidToken(token)) {
                result.ok = false;
                result.error = "ERROR UNKNOWN_TOKEN";
                return result;
            }
            row.push_back(token);
        }
        if (row.empty()) continue;

        if (cols == 0) {
            cols = row.size();
        } else if (row.size() != cols) {
            result.ok = false;
            result.error = "ERROR ROW_WIDTH_MISMATCH";
            return result;
        }
        result.tokens.push_back(row);
    }
    return result;
}

}
