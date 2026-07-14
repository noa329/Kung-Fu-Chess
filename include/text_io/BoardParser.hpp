#ifndef BOARD_PARSER_H
#define BOARD_PARSER_H
#include <vector>
#include <string>
#include <istream>

struct BoardParseResult {
    bool ok;
    std::vector<std::vector<std::string>> tokens;
    std::string error; 
};

namespace BoardParser {
    bool isValidToken(const std::string& token);

    BoardParseResult parse(std::istream& in);
}
#endif
