#include "doctest.h"
#include "TextFieldInput.hpp"

// net_client layer: the pure text-editing logic behind OnlineMenuView's
// login/room-ID fields, testable without a real OpenCV window - see the
// header for why this is split out of OnlineMenuView itself.

TEST_CASE("apply appends a printable character") {
    std::string buf = "ab";
    TextFieldInput::apply(buf, 'c');
    CHECK(buf == "abc");
}

TEST_CASE("apply removes the last character on backspace") {
    std::string buf = "abc";
    TextFieldInput::apply(buf, TextFieldInput::kBackspace);
    CHECK(buf == "ab");
}

TEST_CASE("apply is a no-op backspacing an empty buffer") {
    std::string buf;
    TextFieldInput::apply(buf, TextFieldInput::kBackspace);
    CHECK(buf.empty());
}

TEST_CASE("apply ignores non-printable/control key codes") {
    std::string buf = "ab";
    TextFieldInput::apply(buf, TextFieldInput::kEnter);
    TextFieldInput::apply(buf, TextFieldInput::kEscape);
    TextFieldInput::apply(buf, TextFieldInput::kTab);
    TextFieldInput::apply(buf, -1); // cv::waitKey's "no key pressed"
    CHECK(buf == "ab");
}

TEST_CASE("apply respects maxLen") {
    std::string buf = "ab";
    TextFieldInput::apply(buf, 'c', 2);
    CHECK(buf == "ab");
}
