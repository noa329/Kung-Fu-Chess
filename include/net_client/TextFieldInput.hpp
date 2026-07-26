#ifndef NET_CLIENT_TEXT_FIELD_INPUT_H
#define NET_CLIENT_TEXT_FIELD_INPUT_H
#include <cstddef>
#include <string>

// docs/tasks/graphics-networked-client-plan.md, Task H3b: the pure
// "append/backspace a typed character" logic behind every text-entry field
// in OnlineMenuView (username/password/room-ID) - factored out into its own
// OpenCV-free header so it's unit-testable without a real window.
// OnlineMenuView itself (like BoardView/HudView) is OpenCV-dependent and
// only manually verified, per this project's existing convention for
// renderer-layer views.
//
// Key codes match what Img::show_frame (a thin cv::waitKey wrapper)
// actually returns - confirmed unmasked ASCII already, see
// kungfu-graphics/cpp/src/main.cpp's existing 'm'/27 key checks.
namespace TextFieldInput {

constexpr int kBackspace = 8;
constexpr int kTab = 9;
constexpr int kEnter = 13;
constexpr int kEscape = 27;

// Appends a printable character (32-126) or removes the last one on
// kBackspace; any other key code - including kEnter/kEscape/kTab, and
// cv::waitKey's -1-for-no-key-pressed - leaves `buffer` unchanged. Callers
// check for those separately since they mean "submit"/"back"/"switch
// field", not "edit text".
void apply(std::string& buffer, int key, size_t maxLen = 32);

}
#endif
