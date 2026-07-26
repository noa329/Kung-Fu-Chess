#include "img.hpp"
#include "Board_view.hpp"
#include "Hud_view.hpp"
#include "GameEngine.hpp"
#include "Controller.hpp"
#include "RestDurationLoader.hpp"
#include "SoundManager.hpp"
#include "BoardParser.hpp"
#include <fstream>
#include <iostream>
#include <opencv2/opencv.hpp>

// Cwd-relative, same convention as the server's "data/kungfu_chess.db" and
// "server.log" (see WebSocketServer.cpp) - run the .exe from the repo root.
// Deliberately not a KUNGFU_*_ROOT compile define like ASSETS_ROOT/
// SOUNDS_ROOT below: making the board path launch-directory-independent too
// is a separate, broader robustness task, out of scope here.
static const std::string BOARD_PATH = "boards/standard.txt";

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
static const std::string PIECE_SET = "pieces2";

// Set by CMake to an absolute path to the repo-root assets/sounds folder -
// see target_compile_definitions in kungfu-graphics/cpp/CMakeLists.txt. Same
// "make it work regardless of launch directory" reasoning as ASSETS_ROOT above.
#ifndef KUNGFU_SOUNDS_ROOT
#define KUNGFU_SOUNDS_ROOT "../../assets/sounds"
#endif
static const std::string SOUNDS_ROOT = KUNGFU_SOUNDS_ROOT;

// move.wav/capture.wav are real placeholder recordings already supplied;
// jump.wav and this music file are synthesized placeholders (no MP3 encoder
// was available to produce a real background_music.mp3 - swap this file for
// a real one whenever, .wav and .mp3 both decode fine through SoundManager).
static const std::string MUSIC_RELATIVE_PATH = "background_music.wav";
static const float MUSIC_VOLUME = 0.5f;

// docs/tasks/graphics-networked-client-plan.md, Task H3a: one OS window,
// reused across the mode-select screen, the (future, Task H3b) online-play
// screens, and runLocalGame() - Img::on_mouse/show_frame key off the window
// name, not a window object, so every screen just needs to agree on this
// string rather than passing a window handle around.
static const std::string WINDOW_NAME = "KungFu Chess";

namespace {

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

// Task H3a: startup mode-select screen input - mouse-click buttons, reusing
// the exact same Img::on_mouse callback pattern onMouse() above already
// establishes for gameplay, rather than a second (keyboard-driven) input
// mechanism just for this one screen.
struct ModeSelectContext {
    cv::Rect localButton;
    cv::Rect onlineButton;
    std::string choice; // "" until a button is clicked
};

void onModeSelectMouse(int event, int x, int y, int flags, void* userdata) {
    (void)flags;
    auto* ctx = static_cast<ModeSelectContext*>(userdata);
    if (!ctx || event != cv::EVENT_LBUTTONDOWN) return;

    cv::Point p(x, y);
    if (ctx->localButton.contains(p)) {
        ctx->choice = "local";
    } else if (ctx->onlineButton.contains(p)) {
        ctx->choice = "online";
    }
}

} // namespace

// Blocks until a button is clicked or ESC is pressed. Returns "local",
// "online", or "quit".
std::string chooseMode() {
    const int width = 480;
    const int height = 320;

    ModeSelectContext ctx;
    ctx.localButton = cv::Rect(90, 120, 300, 50);
    ctx.onlineButton = cv::Rect(90, 190, 300, 50);

    Img::on_mouse(WINDOW_NAME, &onModeSelectMouse, &ctx);

    while (ctx.choice.empty()) {
        Img frame(width, height, cv::Scalar(30, 30, 30, 255));
        frame.put_text("Kung Fu Chess", 110, 60, 1.0, cv::Scalar(255, 255, 255, 255), 2);

        for (const auto& button : {std::make_pair(ctx.localButton, std::string("Local Play")),
                                    std::make_pair(ctx.onlineButton, std::string("Online Play"))}) {
            const cv::Rect& r = button.first;
            frame.rectangle(r.x, r.y, r.width, r.height, cv::Scalar(70, 70, 70, 255), -1);
            frame.rectangle(r.x, r.y, r.width, r.height, cv::Scalar(200, 200, 200, 255), 2);
            auto [textW, textH] = frame.text_size(button.second, 0.8, 2);
            frame.put_text(button.second, r.x + (r.width - textW) / 2, r.y + (r.height + textH) / 2, 0.8,
                            cv::Scalar(255, 255, 255, 255), 2);
        }

        int key = frame.show_frame(WINDOW_NAME, 16);
        if (key == 27) return "quit"; // ESC
    }
    return ctx.choice;
}

// Task H3a: stand-in for Task H3b's real login/menu screens, which don't
// exist yet - keeps "Online Play" fully wired end-to-end (drawn AND
// reachable, not just drawn-but-dead) without getting ahead of H3b's own
// scope. Any key returns to chooseMode().
void showOnlinePlayStub() {
    Img frame(480, 320, cv::Scalar(30, 30, 30, 255));
    frame.put_text("Online Play", 140, 120, 1.0, cv::Scalar(255, 255, 255, 255), 2);
    frame.put_text("Coming soon - see docs/tasks/", 55, 170, 0.55, cv::Scalar(200, 200, 200, 255), 1);
    frame.put_text("graphics-networked-client-plan.md", 55, 195, 0.55, cv::Scalar(200, 200, 200, 255), 1);
    frame.put_text("Press any key to go back", 100, 250, 0.6, cv::Scalar(150, 150, 150, 255), 1);
    frame.show_frame(WINDOW_NAME, 0); // 0 = wait indefinitely for any key
}

// Task H3a: today's entire local-play loop, extracted out of main()
// unchanged - BoardParser -> GameEngine -> Controller -> BoardView/HudView
// -> EventBus wiring is byte-for-byte identical to what main() used to do
// directly (only the window-name variable moved to file scope, see
// WINDOW_NAME above). Local Play must keep behaving exactly as it did
// before this file grew a mode-select screen in front of it - including
// ESC here still ending the whole program, not returning to a menu.
int runLocalGame() {
    BoardView view;
    if (!view.init(ASSETS_ROOT, PIECE_SET)) {
        std::cerr << "Failed to load board/pieces from \"" << ASSETS_ROOT
                  << "\". Check KUNGFU_ASSETS_ROOT (see CMakeLists.txt) or ASSETS_ROOT in main.cpp."
                  << std::endl;
        return 1;
    }

    std::ifstream boardFile(BOARD_PATH);
    if (!boardFile.is_open()) {
        std::cerr << "Failed to open starting position file \"" << BOARD_PATH
                  << "\". Run the executable from the repo root." << std::endl;
        return 1;
    }
    BoardParseResult boardResult = BoardParser::parse(boardFile);
    if (!boardResult.ok || boardResult.tokens.empty()) {
        std::cerr << "Failed to parse starting position from \"" << BOARD_PATH << "\": "
                  << (boardResult.ok ? "file has no board rows" : boardResult.error)
                  << std::endl;
        return 1;
    }

    GameEngine engine;
    engine.startGame(boardResult.tokens);

    // Real long_rest/short_rest durations derived from PIECE_SET's own
    // sprite config, replacing RealTimeArbiter's hardcoded 800/500ms
    // guesses. Falls back to those defaults (leaves setRestDurations
    // uncalled) if the representative sprite folder is missing/unreadable.
    if (auto restDurations = computeRestDurationsFromSprites(ASSETS_ROOT, PIECE_SET, "PW")) {
        engine.setRestDurations(restDurations->longRestMs, restDurations->shortRestMs);
        std::cout << "Rest durations from " << PIECE_SET << ": long_rest="
                  << restDurations->longRestMs << "ms short_rest="
                  << restDurations->shortRestMs << "ms" << std::endl;
    } else {
        std::cerr << "Warning: could not compute rest durations from " << PIECE_SET
                  << "/PW sprites; using the built-in 800ms/500ms defaults." << std::endl;
    }
    Controller controller(engine, view.cellSize());
    HudView hud;
    MouseContext mouseCtx{&controller, &hud};

    SoundManager::instance().setSoundsRoot(SOUNDS_ROOT);
    SoundManager::instance().playMusic(MUSIC_RELATIVE_PATH, MUSIC_VOLUME);

    // EventBus wiring: GameEngine publishes without knowing who's
    // listening; this composition root is the only place that connects
    // its events to concrete subscribers (SoundManager, HudView).
    engine.events().onSound.subscribe([](const SoundEvent& e) {
        SoundManager::instance().playSound(e.name + ".wav");
    });
    engine.events().onGameLifecycle.subscribe([&hud](const GameLifecycleEvent& e) {
        if (e.phase == "end") {
            hud.playEndAnimation(e.result);
        }
    });

    Img::on_mouse(WINDOW_NAME, &onMouse, &mouseCtx);

    int64 last_tick = cv::getTickCount();
    const double tick_freq = cv::getTickFrequency();

    std::cout << "Left-click a piece, then left-click a destination square to move it." << std::endl;
    std::cout << "Right-click a piece to make it jump. Press 'M' to mute/unmute sound. Press ESC to quit." << std::endl;

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
        int key = frame.show_frame(WINDOW_NAME, 16); // ~60 FPS poll
        if (key == 27) { // ESC
            break;
        } else if (key == 'm' || key == 'M') {
            SoundManager::instance().toggleEnabled();
            if (SoundManager::instance().isEnabled()) {
                SoundManager::instance().playMusic(MUSIC_RELATIVE_PATH, MUSIC_VOLUME);
            } else {
                SoundManager::instance().stopMusic();
            }
        }
    }

    return 0;
}

int main() {
    try {
        std::string mode = chooseMode();
        while (mode == "online") {
            showOnlinePlayStub();
            mode = chooseMode();
        }
        if (mode == "quit") {
            return 0;
        }
        return runLocalGame();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
