#include "img.hpp"
#include "Board_view.hpp"
#include "Hud_view.hpp"
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

struct MouseContext {
    Controller* controller;
    HudView* hud;
};

// EVENT_LBUTTONDOWN selects/moves (Controller::handleClick), EVENT_RBUTTONDOWN
// triggers a jump in place (Controller::handleJump), EVENT_MOUSEWHEEL scrolls
// whichever move-log panel the cursor is over. Click/jump coordinates are
// corrected by the HUD's board origin before reaching Controller, since the
// HUD canvas is wider than the board itself - Controller still owns the only
// pixel->Position conversion, this just adjusts the input coordinate frame.
void onMouse(int event, int x, int y, int flags, void* userdata) {
    auto* ctx = static_cast<MouseContext*>(userdata);
    if (!ctx || !ctx->controller || !ctx->hud) return;

    if (event == cv::EVENT_LBUTTONDOWN) {
        ctx->controller->handleClick(x - ctx->hud->boardOriginX(), y - ctx->hud->boardOriginY());
    } else if (event == cv::EVENT_RBUTTONDOWN) {
        ctx->controller->handleJump(x - ctx->hud->boardOriginX(), y - ctx->hud->boardOriginY());
    } else if (event == cv::EVENT_MOUSEWHEEL) {
        ctx->hud->handleScroll(x, y, cv::getMouseWheelDelta(flags));
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
        HudView hud;
        MouseContext mouseCtx{&controller, &hud};

        const std::string window_name = "KungFu Chess";
        Img::on_mouse(window_name, &onMouse, &mouseCtx);

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

            Img boardFrame = view.render(snap);
            Img frame = hud.compose(boardFrame, snap);
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
