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
#include "NetworkController.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <variant>
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

// docs/tasks/graphics-networked-client-plan.md, Task H4/H5: online-play
// gameplay screen's mouse handling - same event/coordinate-correction shape
// as onMouse() above, but a null `controller` (a spectator - Task H5's own
// "spectator mode disables click sending entirely") makes every event a
// silent no-op instead of forwarding to NetworkController.
struct NetworkMouseContext {
    NetworkController* controller = nullptr;
    HudView* hud = nullptr;
};

void onNetworkMouse(int event, int x, int y, int flags, void* userdata) {
    auto* ctx = static_cast<NetworkMouseContext*>(userdata);
    if (!ctx || !ctx->hud) return;

    if (event == cv::EVENT_LBUTTONDOWN) {
        if (ctx->controller) {
            ctx->controller->handleClick(x - ctx->hud->boardOriginX(), y - ctx->hud->boardOriginY());
        }
    } else if (event == cv::EVENT_RBUTTONDOWN) {
        if (ctx->controller) {
            ctx->controller->handleJump(x - ctx->hud->boardOriginX(), y - ctx->hud->boardOriginY());
        }
    } else if (event == cv::EVENT_MOUSEWHEEL) {
        // A spectator (null controller) can still scroll the move-log
        // panels - only sending commands is gated on having a color.
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

    // Task H6: SoundManager is a singleton shared with Local Play, but
    // runLocalGame() (the only prior caller of setSoundsRoot()) never runs
    // on this path - a player who only ever picks Online Play would
    // otherwise hit playSound()'s untouched "assets/sounds" default and
    // silently fail to resolve any file. No playMusic() call here though:
    // background music is a Local Play concern this task doesn't touch.
    SoundManager::instance().setSoundsRoot(SOUNDS_ROOT);

    std::cout << "Left-click a piece, then left-click a destination square to move it." << std::endl;
    std::cout << "Right-click a piece to make it jump. Press 'R' to resign. Press ESC to disconnect." << std::endl;

    // Task H5: same BoardView/HudView Local Play uses, fed from network
    // state instead of a local GameEngine - initialized upfront (same as
    // runLocalGame()) since it doesn't depend on anything network-related,
    // not lazily on first reaching the game screen.
    BoardView boardView;
    if (!boardView.init(ASSETS_ROOT, PIECE_SET)) {
        std::cerr << "Failed to load board/pieces from \"" << ASSETS_ROOT
                  << "\" for Online Play. Check KUNGFU_ASSETS_ROOT (see CMakeLists.txt) or ASSETS_ROOT in main.cpp."
                  << std::endl;
        return;
    }
    HudView hud;
    // Constructed once myColor is known (Task H4) - absent entirely for a
    // spectator, who has nothing to send (see NetworkMouseContext's own
    // null-controller handling above).
    std::optional<NetworkController> networkController;
    GameSnapshot currentSnapshot{}; // empty board until the first real tick arrives
    // Task H7: coalesced "latest value" cache, same reasoning/pattern as
    // currentSnapshot above - a nullopt poll means "no new tick this
    // frame," not "no one's disconnected," so this has to be cached and
    // re-passed to hud.compose() every frame, not just on the frames a new
    // value actually arrives.
    DisconnectStatus currentDisconnectStatus{};
    bool hasSnapshot = false;
    // Bugfix (regression from H5): a room creator's own board tick can
    // arrive before anyone else joins (handleCreateRoom() broadcasts the
    // freshly loaded starting position immediately - see its own comment -
    // so the creator isn't staring at nothing while alone), so hasSnapshot
    // alone was the wrong signal for leaving the "share this room ID"
    // status screen - it left almost instantly, before the ID could be
    // read. Set true only when the creator explicitly asks to see the
    // board before an opponent has joined (see the "status_connected"
    // branch below) - otherwise that screen stays up until the opponent
    // actually joins (opponentName stops being blank).
    bool skippedWaitingForOpponent = false;
    int64 lastTick = cv::getTickCount();
    const double tickFreq = cv::getTickFrequency();

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

                // Task H4: a spectator never gets a click-sending
                // controller - GameSession::handleCommand() would reject
                // their commands anyway (ERROR NOT_A_PLAYER), but this
                // keeps clicks from even trying to build one.
                if (myColor == "white" || myColor == "black") {
                    char colorChar = (myColor == "white") ? 'w' : 'b';
                    networkController.emplace(client, colorChar, boardView.cellSize());
                }
                lastTick = cv::getTickCount(); // dt starts counting from here, not from connect()
            }
        } else if (screen == "status_connected") {
            // An unexpected server-side close (crash, kicked, network
            // drop) - route to a dismissible message instead of silently
            // spinning on a frame that will never update again (see the
            // plan's threading-model section).
            if (!client.isConnected()) {
                screen = "disconnected";
                continue;
            }

            // Task H6: drain every discrete sound/lifecycle push queued
            // since last frame - same effect as runLocalGame()'s direct
            // EventBus subscriptions, different trigger source (there's no
            // local EventBus/GameEngine on this path, see OnlineClient's
            // own header comment). Mirrors runLocalGame()'s
            // onGameLifecycle subscriber exactly: only phase == "end"
            // triggers the banner, matching GameLifecycleEvent's own
            // documented meaning (a "start" push has no meaningful result
            // string to show).
            for (auto& event : client.pollEvents()) {
                if (std::holds_alternative<SoundEvent>(event)) {
                    SoundManager::instance().playSound(std::get<SoundEvent>(event).name + ".wav");
                } else {
                    const auto& lifecycle = std::get<GameLifecycleEvent>(event);
                    if (lifecycle.phase == "end") {
                        hud.playEndAnimation(lifecycle.result);
                    }
                }
            }

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

            // Task H5: consume whatever the latest state-tick broadcast
            // is; a nullopt poll just means no *new* tick arrived this
            // frame, not that there's nothing to show - keep rendering
            // currentSnapshot from the last real one either way.
            if (auto snap = client.pollGameState()) {
                currentSnapshot = std::move(*snap);
                hasSnapshot = true;
            }

            // Task H7: same coalescing rule as pollGameState() above - keep
            // showing the last known countdown on a nullopt poll, don't
            // treat it as "no one's disconnected."
            if (auto status = client.pollDisconnectStatus()) {
                currentDisconnectStatus = *status;
            }

            // Bugfix: only a room creator ever has both a roomId *and* a
            // still-blank opponent at the same time (matchmaking has no
            // roomId at all; a room joiner's own "joined" response already
            // carries the opponent's name) - so this is specifically "the
            // room exists, but no one's joined it yet," not a general
            // "state not ready" case hasSnapshot already covers below.
            bool waitingForOpponent = !roomId.empty() && opponentName.empty() && !skippedWaitingForOpponent;
            if (waitingForOpponent) {
                view.renderStatus(WINDOW_NAME, "Connected",
                                   describeConnection(myColor, opponentName, roomId, whiteUsername, blackUsername)
                                       + "\nWaiting for an opponent to join...\n"
                                         "Press any key to view the board while you wait.",
                                   true);
                if (view.consumeBackRequest()) {
                    // "Dismiss" means "show me the board early" here, not
                    // "go back" - unlike every other screen's use of this
                    // same signal (see OnlineMenuView's own doc comment).
                    skippedWaitingForOpponent = true;
                }
                continue;
            }

            if (!hasSnapshot) {
                // Briefly between "joined" and the session's first
                // broadcastState() tick - a real (if short-lived) state,
                // not an error.
                view.renderStatus(WINDOW_NAME, "Connected",
                                   describeConnection(myColor, opponentName, roomId, whiteUsername, blackUsername)
                                       + "\nWaiting for the first game state...",
                                   false);
                continue;
            }

            // Task H2 decision: whiteName/blackName never arrive over the
            // wire (that data belongs to "joined"/room messages, not the
            // state-tick broadcast) - filled in here from what the login/
            // join flow already established, closing that gap now that
            // there's a HUD on screen to show it in. Falls back to this
            // client's own logged-in `username` for its own seat when the
            // opponent isn't known yet (only reachable via the early-dismiss
            // path above - normally both names are already known together,
            // the instant opponentName stops being blank) - own name should
            // never be blank just because the opponent's still is.
            currentSnapshot.whiteName = !whiteUsername.empty() ? whiteUsername : (myColor == "white" ? username : "");
            currentSnapshot.blackName = !blackUsername.empty() ? blackUsername : (myColor == "black" ? username : "");
            // Task H4: GameSnapshot::selected is never sent over the wire
            // either (Task H2 decision 1) - substituting this client's own
            // pending click reconstructs the highlight for a real player;
            // a spectator (no networkController) just never highlights
            // anything, which is correct - they have no pending click.
            currentSnapshot.selected =
                networkController && networkController->hasPendingSelection()
                    ? networkController->pendingSelection()
                    : Position{-1, -1};

            int64 now = cv::getTickCount();
            int dtMs = static_cast<int>((now - lastTick) * 1000.0 / tickFreq);
            lastTick = now;

            boardView.syncFromSnapshot(currentSnapshot);
            boardView.update(dtMs);

            Img boardFrame = boardView.render(currentSnapshot);
            Img frame = hud.compose(boardFrame, currentSnapshot, currentDisconnectStatus);

            // Static, not a per-frame stack local: cv::setMouseCallback's
            // registration would otherwise point at freed stack memory the
            // instant this scope ends - the exact dangling-pointer bug
            // chooseMode()'s own ModeSelectContext fix (Task H3b) already
            // found and fixed for the same reason. Mutated in place every
            // frame instead, matching that fix's pattern.
            static NetworkMouseContext mouseCtx;
            mouseCtx.controller = networkController ? &*networkController : nullptr;
            mouseCtx.hud = &hud;
            if (networkController) networkController->setSnapshot(&currentSnapshot);
            Img::on_mouse(WINDOW_NAME, &onNetworkMouse, &mouseCtx);

            int key = frame.show_frame(WINDOW_NAME, 16);
            if (key == 27) { // ESC - disconnect and return to the Local/Online menu
                client.disconnect();
                return;
            } else if ((key == 'r' || key == 'R') && networkController) {
                // Task H7: literal "resign" command, symmetric with
                // client/cli's own typed "resign" line - GameCommandParser
                // server-side (Task G3) is the sole source of truth for
                // validity, so no client-side gameOver/legality check here,
                // same reasoning client/cli's gameplay loop already uses.
                // Gated on networkController only so a spectator (no seat
                // to resign) doesn't send a command the server would just
                // reject as NOT_A_PLAYER anyway.
                client.sendCommand("resign");
            }
        } else if (screen == "disconnected") {
            view.renderStatus(WINDOW_NAME, "Disconnected",
                               "Connection to the server was lost.\nPress any key to go back.", true);
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
