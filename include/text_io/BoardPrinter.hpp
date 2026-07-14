#ifndef BOARD_PRINTER_H
#define BOARD_PRINTER_H
#include <vector>
#include <string>
#include <ostream>

namespace BoardPrinter {
    void print(const std::vector<std::vector<std::string>>& tokens, std::ostream& out);
}
#endif
