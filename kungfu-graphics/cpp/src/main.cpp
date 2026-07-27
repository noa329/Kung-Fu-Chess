#include "img.hpp"
#include "Board_view.hpp"
#include "Hud_view.hpp"
#include "GameEngine.hpp"
#include "Controller.hpp"
#include "RestDurationLoader.hpp"
#include "SoundManager.hpp"
#include "BoardParser.hpp"
#include "OnlineMenuView.hpp"
#include "OnlineFlowState.hpp"
#include "OnlineClient.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
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

// docs/tasks/graphics-networked-client-plan.md, Task H3b: matches
// server/main.cpp's port and client/cli's own default - hardcoded rather
// than a command-line argument (unlike client/cli, this .exe is normally
// launched via VS Code/CMake Tools with no argv, so there'd be nowhere
// convenient to pass one).
static const std::string SERVER_URI = "ws://127.0.0.1:9002/";

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
//
// Task H3b bugfix: `ctx` used to be a plain stack-local here. Once
// chooseMode() returns, cv::setMouseCallback's registration still points
// at it - a dangling pointer into freed stack memory - until some *later*
// call re-registers the callback. main()'s `while (mode == "online")` loop
// calls chooseMode() more than once per run, and in between (while
// runOnlineGame()'s own screens are up) nothing was re-registering the
// callback, so a stray click during that window would have read/written
// through a dangling pointer - undefined behavior, not just a cosmetic
// bug. Fixed by giving `ctx` static storage duration instead (stable for
// the whole program, never dangling), clearing it explicitly at the top
// of every call so a fresh invocation doesn't see a stale choice from a
// previous one.
std::string chooseMode() {
    const int width = 480;
    const int height = 320;

    static ModeSelectContext ctx;
    ctx.choice.clear();
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

namespace {

// Status text for OnlineMenuView::renderStatus() while a request is in
// flight - one line per pendingAction, matching what client/cli prints at
// the same points in its own login/matchmaking flow.
std::string waitingMessageFor(const std::string& pendingAction) {
    if (pendingAction == "login") return "Logging in...";
    if (pendingAction == "play") return "Searching for an opponent...";
    if (pendingAction == "create_room") return "Creating room...";
    if (pendingAction == "join_room") return "Joining room...";
    return "Please wait...";
}

// Status text for the final "connected" screen - mirrors client/cli's own
// three-way color/opponent/spectator message shape (see its
// handleServerMessage()'s "joined"/"reconnected" branches).
std::string describeConnection(const std::string& color, const std::string& opponent, const std::string& roomId,
                                const std::string& whiteUsername, const std::string& blackUsername) {
    std::ostringstream out;
    if (!roomId.empty()) {
        out << "Room ID: " << roomId << " - share this with your opponent.\n";
    }
    if (color == "white") {
        out << "You are White. Playing against " << opponent << ".";
    } else if (color == "black") {
        out << "You are Black. Playing against " << opponent << ".";
    } else {
        out << "Spectating: " << whiteUsername << " (White) vs " << blackUsername << " (Black).";
    }
    out << "\nGameplay wiring lands in a later task - press any key to disconnect.";
    return out.str();
}

} // namespace

// docs/tasks/graphics-networked-client-plan.md, Task H3b: the real Online
// Play flow - login through a successful match/room join, replacing H3a's
// "coming soon" stub. Per the plan's confirmed decision, stops at a
// "connected" status screen and *stays connected* rather than
// disconnecting immediately - actually rendering the game from network
// state is Task H5's job, reusing this same OnlineClient connection
// rather than opening a new one.
//
// The screen-transition logic itself is delegated to
// OnlineFlowState::nextScreenAfterResponse() (net_client, pure/tested) so
// this function stays a thin loop: draw whichever screen `screen` names,
// poll OnlineClient non-blockingly, react. Never blocks on the network -
// see the plan's threading-model section for why (the window must keep
// pumping frames during a real round trip).
void runOnlineGame() {
    OnlineClient client;
    OnlineMenuView view;
    client.connect(SERVER_URI);

    std::string screen = "connecting";
    std::string pendingAction;
    std::string username;
    std::string errorMessage;
    std::string myColor, opponentName, roomId, whiteUsername, blackUsername;

    while (true) {
        if (screen == "connecting") {
            auto opened = client.pollConnectionOpened();
            if (opened.has_value()) {
                screen = *opened ? "login" : "connection_failed";
                continue;
            }
            view.renderStatus(WINDOW_NAME, "Online Play", "Connecting to " + SERVER_URI + " ...", false);
        } else if (screen == "connection_failed") {
            view.renderStatus(WINDOW_NAME, "Connection Failed",
                               "Could not reach the server.\nPress any key to go back.", true);
            if (view.consumeBackRequest()) return;
        } else if (screen == "login") {
            auto submission = view.renderLogin(WINDOW_NAME, errorMessage);
            if (view.consumeBackRequest()) {
                client.disconnect();
                return;
            }
            if (submission) {
                username = submission->username;
                client.sendLogin(submission->username, submission->password);
                pendingAction = "login";
                errorMessage.clear();
                screen = "waiting";
            }
        } else if (screen == "menu") {
            auto choice = view.renderMenu(WINDOW_NAME, username);
            if (view.consumeBackRequest()) {
                client.disconnect();
                return;
            }
            if (choice == "quick_match") {
                client.sendPlay();
                pendingAction = "play";
                screen = "waiting";
            } else if (choice == "create_room") {
                client.sendCreateRoom();
                pendingAction = "create_room";
                screen = "waiting";
            } else if (choice == "join_room") {
                screen = "room_id_entry";
                view.reset();
            }
        } else if (screen == "room_id_entry") {
            auto roomIdInput = view.renderTextPrompt(WINDOW_NAME, "Enter room ID:");
            if (view.consumeBackRequest()) {
                screen = "menu";
                view.reset();
                continue;
            }
            if (roomIdInput) {
                client.sendJoinRoom(*roomIdInput);
                pendingAction = "join_room";
                screen = "waiting";
            }
        } else if (screen == "waiting") {
            // Not dismissible: a request is genuinely in flight, and
            // there's no safe way to "cancel" it client-side without
            // risking the eventual late response being misread as the
            // answer to whatever's sent next - see OnlineClient's own
            // "only one request in flight at a time" assumption.
            view.renderStatus(WINDOW_NAME, "Online Play", waitingMessageFor(pendingAction), false);

            auto resp = client.pollResponse();
            if (!resp) continue;

            if (!resp->ok) errorMessage = resp->error;
            screen = OnlineFlowState::nextScreenAfterResponse(pendingAction, resp->ok, resp->reconnected);
            if (screen == "login" || screen == "menu" || screen == "room_id_entry") {
                view.reset();
            } else if (screen == "status_connected") {
                myColor = resp->color;
                opponentName = resp->opponent;
                roomId = resp->roomId;
                whiteUsername = resp->whiteUsername;
                blackUsername = resp->blackUsername;
            }
        } else if (screen == "status_connected") {
            // Bugfix: the room-creator path can receive a *second* "joined"
            // message here, once an opponent actually joins the room (see
            // WebSocketServer::handleJoinRoom()'s own fix) - the creator's
            // first (and, before this fix, only) "joined"-equivalent
            // response is "room_created", sent back when the room is still
            // empty, so its opponent field is necessarily blank. Matchmaking
            // and the room-joiner path never send a second response here -
            // this poll is a harmless no-op for them, not a new wait.
            if (auto resp = client.pollResponse()) {
                if (resp->ok) {
                    opponentName = resp->opponent;
                    whiteUsername = resp->whiteUsername;
                    blackUsername = resp->blackUsername;
                }
            }
            view.renderStatus(WINDOW_NAME, "Connected",
                               describeConnection(myColor, opponentName, roomId, whiteUsername, blackUsername), true);
            if (view.consumeBackRequest()) {
                client.disconnect();
                return;
            }
        }
    }
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
            runOnlineGame();
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
