#include "TextFieldInput.hpp"

namespace TextFieldInput {

void apply(std::string& buffer, int key, size_t maxLen) {
    if (key == kBackspace) {
        if (!buffer.empty()) buffer.pop_back();
    } else if (key >= 32 && key <= 126 && buffer.size() < maxLen) {
        buffer += static_cast<char>(key);
    }
}

}
