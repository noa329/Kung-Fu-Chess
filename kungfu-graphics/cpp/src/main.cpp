#include "img.hpp"
#include "Board_view.hpp"
#include "GameEngine.hpp"
#include "Controller.hpp"
#include <iostream>
#include <opencv2/opencv.hpp>

// Set by CMake to an absolute path to the kungfu-graphics folder (contains
// board.png, pieces1/, pieces2/) - see target_compile_definitions in
// kungfu-graphics/cpp/CMakeLists.txt. This makes the program work no matter
// what the current working directory is when you press Run/Debug.
// The "../../kungfu-graphics" fallback only works if you happen to launch
// the .exe from its own build output folder.
#ifndef KUNGFU_ASSETS_ROOT
#define KUNGFU_ASSETS_ROOT "../../kungfu-graphics"
#endif
static const std::string ASSETS_ROOT = KUNGFU_ASSETS_ROOT;

namespace {

std::vector<std::vector<std::string>> standardStartingPosition() {
    return {
        {"bR", "bN", "bB", "bQ", "bK", "bB", "bN", "bR"},
        {"bP", "bP", "bP", "bP", "bP", "bP", "bP", "bP"},
        {".",  ".",  ".",  ".",  ".",  ".",  ".",  "."},
        {".",  ".",  ".",  ".",  ".",  ".",  ".",  "."},
        {".",  ".",  ".",  ".",  ".",  ".",  ".",  "."},
        {".",  ".",  ".",  ".",  ".",  ".",  ".",  "."},
        {"wP", "wP", "wP", "wP", "wP", "wP", "wP", "wP"},
        {"wR", "wN", "wB", "wQ", "wK", "wB", "wN", "wR"},
    };
}

// EVENT_LBUTTONDOWN selects/moves (Controller::handleClick), EVENT_RBUTTONDOWN
// triggers a jump in place (Controller::handleJump) - userdata is a Controller*.
void onMouse(int event, int x, int y, int /*flags*/, void* userdata) {
    auto* controller = static_cast<Controller*>(userdata);
    if (!controller) return;

    if (event == cv::EVENT_LBUTTONDOWN) {
        controller->handleClick(x, y);
    } else if (event == cv::EVENT_RBUTTONDOWN) {
        controller->handleJump(x, y);
    }
}

} // namespace

int main() {
    try {
        BoardView view;
        if (!view.init(ASSETS_ROOT, "pieces2")) {
            std::cerr << "Failed to load board/pieces from \"" << ASSETS_ROOT
                      << "\". Check KUNGFU_ASSETS_ROOT (see CMakeLists.txt) or ASSETS_ROOT in main.cpp."
                      << std::endl;
            return 1;
        }

        GameEngine engine;
        engine.loadBoard(standardStartingPosition());
        Controller controller(engine, view.cellSize());

        const std::string window_name = "KungFu Chess";
        Img::on_mouse(window_name, &onMouse, &controller);

        int64 last_tick = cv::getTickCount();
        const double tick_freq = cv::getTickFrequency();

        std::cout << "Left-click a piece, then left-click a destination square to move it." << std::endl;
        std::cout << "Right-click a piece to make it jump. Press ESC to quit." << std::endl;

        while (true) {
            int64 now = cv::getTickCount();
            int dt_ms = static_cast<int>((now - last_tick) * 1000.0 / tick_freq);
            last_tick = now;

            engine.wait(dt_ms); // resolve any moves/jumps whose time is up

            GameSnapshot snap = engine.snapshot();
            view.syncFromSnapshot(snap);
            view.update(dt_ms);

            Img frame = view.render(snap);
            int key = frame.show_frame(window_name, 16); // ~60 FPS poll
            if (key == 27) { // ESC
                break;
            }
        }

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
