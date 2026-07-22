#ifndef SERVER_WEBSOCKET_SERVER_H
#define SERVER_WEBSOCKET_SERVER_H
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#include <asio.hpp>
#include "GameSession.hpp"
#include "SessionManager.hpp"
#include "MatchmakingQueue.hpp"
#include "Logger.hpp"
#include "Database.hpp"
#include "UserRepository.hpp"
#include "AuthService.hpp"
#include <cstdint>
#include <memory>
#include <chrono>
#include <fstream>
#include <map>
#include <utility>
#include <vector>

// Wires GameCommandParser (A2) + GameSession (A3) + GameStateSerializer
// (A4) together over a real WebSocket connection.
//
// Task D1: generalized from A5's single hardcoded GameSession to N
// concurrent ones, routed via SessionManager.
//
// Task D3: real matchmaking, replacing A5/B2's "connect and immediately
// join the one pre-existing session" lifecycle entirely. The new
// connection lifecycle:
//   1. onOpen(): accepted unconditionally - no session exists yet, so
//      there's nothing to cap here any more.
//   2. `{"type":"join","username":...,"password":...}` - login only, via
//      AuthService directly (no GameSession involved - there isn't one
//      yet). Responds `{"type":"logged_in","username":...}` or
//      `{"type":"join_rejected","error":...}`. No color, no session.
//   3. `{"type":"play"}` - enqueues the now-authenticated connection into
//      matchmakingQueue_ with their current rating and starts a 60s
//      steady_timer (Task D3). Responds `{"type":"searching"}` if not
//      immediately matched.
//   4. On a match (checked once per enqueue - see handlePlay()): a new
//      session is created via createSession(), both matched connections
//      are added to it, each gets `{"type":"joined","color":...,
//      "username":...,"opponent":...}` - matching is atomic now (both
//      players known at the same moment), so the old B2-era
//      `opponent_joined` notification and the hdlToColor_ map it needed
//      are both gone; GameSession::assignSeat() is called directly with
//      each username, not GameSession::handleJoin() (removed).
//   5. On a 60s timeout with no match: dequeued, sent
//      `{"type":"matchmaking_timeout","error":"ERROR NO_OPPONENT_FOUND"}`.
//   6. Once in a session: game commands route through GameCommandParser +
//      GameSession::handleCommand(), same as always.
//
// Task D4: disconnect/reconnect, layered onto the above without changing
// it. If a connection inside an unfinished session's seat closes (socket
// close/fail - onClose()), that seat is marked disconnected
// (GameSession::markDisconnected), its hdl is freed from SessionManager
// (so a reconnect can tryAdd() back in), and a 20s steady_timer starts
// (same real-elapsed-time-measuring pattern as D3's 60s matchmaking
// timeout). Every broadcastState() tick in the meantime carries that
// seat's remaining ms (GameStateSerializer::DisconnectStatus) so the
// still-connected opponent's client can render a countdown - no separate
// per-second broadcast loop. If `{"type":"join",...}` re-authenticates as
// that same username before the timer fires (tryReconnect(), called from
// handleLogin()), the seat is marked reconnected and the timer cancelled;
// otherwise the timer's callback resigns that color
// (GameEngine::resign()) and the game ends exactly like a king capture
// would. A connection that closes while only queued in matchmakingQueue_
// (not yet matched into a session) is just dequeued - D4's reconnect
// support is specifically for a game already in progress, not the search
// queue, which already has its own independent 60s give-up timeout (D3).
//
// Task C3: Database/UserRepository/AuthService stay process-wide, not
// per-session - data/kungfu_chess.db, created by server/main.cpp before
// this class is constructed (SQLite can create the .db file itself but
// not a missing parent directory).
//
// This is the one place in server/ that touches websocketpp/Asio
// directly, so unlike GameCommandParser/GameSession/GameStateSerializer
// it is NOT dual-compiled - excluded from the Makefile build via
// SERVER_ONLY_SRC (mirrors the renderer's OPENCV_ONLY_SRC).
//
// ASIO_STANDALONE and _WEBSOCKETPP_CPP11_THREAD_ come from
// server/CMakeLists.txt's target_compile_definitions, not defined here.
class WebSocketServer {
public:
    using server_t = websocketpp::server<websocketpp::config::asio>;
    using hdl_compare = std::owner_less<websocketpp::connection_hdl>;

    explicit WebSocketServer(uint16_t port);

    // Blocks, running the asio event loop (accept + message handling +
    // the periodic engine tick) until the process is killed.
    void run();

private:
    static constexpr size_t kMaxConnectionsPerSession = 2;
    // How often the timer fires - NOT how much simulated time passes per
    // tick. That's measured separately (lastTickTime_) and fed to
    // engine().wait() as the real elapsed duration: the real tick period
    // reliably runs slower than this nominal value (Windows timer
    // granularity + asio/websocketpp overhead measured around ~30ms
    // actual for a 16ms request during manual verification), and hardcoding
    // this constant as "simulated ms per tick" made the engine's simulated
    // clock run at roughly half real-time speed - confirmed as the actual
    // cause of a move appearing to never resolve during Task A5 testing,
    // not a game-logic bug. Same dt-from-real-elapsed-time pattern the
    // graphics main.cpp's render loop already uses (cv::getTickCount()-based
    // dt_ms), not a new invention.
    static constexpr int kTickMs = 16;

    // Task D3: a successfully logged-in (but not yet matched) connection's
    // identity - just enough to enqueue into matchmaking and to assign a
    // seat by username once matched.
    struct AuthenticatedUser {
        std::string username;
        int rating;
    };

    // Task D3: one connection currently waiting in matchmakingQueue_.
    // enqueuedAt is real wall-clock time (std::chrono::steady_clock, not
    // the timer's nominal deadline) so the timeout callback measures
    // actual elapsed time - same "don't trust the nominal value" lesson
    // A5's own tick-timing bug already established.
    struct QueuedSeeker {
        websocketpp::connection_hdl hdl;
        std::chrono::steady_clock::time_point enqueuedAt;
        std::unique_ptr<asio::steady_timer> timer;
    };

    // Task D4: one seat currently disconnected and counting down toward
    // auto-resign. disconnectedAt is real wall-clock time, same
    // don't-trust-the-nominal-timer-value reasoning as QueuedSeeker above.
    struct DisconnectedSeat {
        std::chrono::steady_clock::time_point disconnectedAt;
        std::unique_ptr<asio::steady_timer> timer;
    };

    uint16_t port_;
    server_t server_;
    // Opened before logger_ (member init order = declaration order) so the
    // stream is valid by the time Logger's sink list references it -
    // logFile_ must outlive logger_, and logger_ must outlive every
    // GameSession's use of it (attachLogger() just stores a pointer,
    // doesn't extend lifetime), hence this exact declaration order.
    std::ofstream logFile_;
    Logger logger_;
    // Declaration order matters: database_ must outlive userRepository_'s
    // reference to it, userRepository_ must outlive authService_'s
    // reference to it.
    Database database_;
    UserRepository userRepository_;
    AuthService authService_;
    // Task D1: connection -> session routing (pure, see SessionManager.hpp)
    // plus the actual GameSession instances, parallel-indexed by
    // SessionManager's session ids. unique_ptr so pointers/references into
    // sessions_ stay stable as the vector grows (GameSession itself is
    // neither copyable nor cheaply movable, and doesn't need to be). Task
    // D3: starts empty - a session is only ever created by handleMatch(),
    // not at startup any more.
    SessionManager<websocketpp::connection_hdl, hdl_compare> sessionManager_;
    std::vector<std::unique_ptr<GameSession>> sessions_;
    std::unique_ptr<asio::steady_timer> tickTimer_;
    std::chrono::steady_clock::time_point lastTickTime_;

    // Task D3: logged-in-but-not-yet-matched connections.
    std::map<websocketpp::connection_hdl, AuthenticatedUser, hdl_compare> authenticatedUsers_;
    // Task D3: pure pairing decision (Task D2) - keyed by a small
    // locally-allocated int, not connection_hdl directly, since
    // connection_hdl (a std::weak_ptr<void>) has no operator== for
    // MatchmakingQueue's remove()/contains() to use (the identity problem
    // Task D2 deliberately deferred until this point).
    MatchmakingQueue<int> matchmakingQueue_;
    int nextSeekerId_ = 0;
    std::map<int, QueuedSeeker> queuedSeekers_;
    std::map<websocketpp::connection_hdl, int, hdl_compare> hdlToSeekerId_;

    // Task D4: which session a username is (or was last) matched into -
    // populated in handleMatch() alongside sessions_/sessionManager_, and
    // consulted on a later login to decide "is this a fresh matchmaking
    // login, or a reconnect into a game already in progress". Never
    // erased (mirrors SessionManager's own no-removeSession() stance -
    // sessions aren't torn down once created); reconnect eligibility is
    // gated on the seat actually being marked disconnected AND the game
    // not already being over, not on map membership alone.
    std::map<std::string, int> usernameToSessionId_;
    // Task D4: disconnected seats currently counting down toward
    // auto-resign, keyed by (sessionId, color).
    std::map<std::pair<int, char>, DisconnectedSeat> disconnectedSeats_;

    // Creates a new session: SessionManager bookkeeping plus the actual
    // GameSession (logger attached, standard board loaded). Only ever
    // called from handleMatch() - see the class comment above.
    int createSession();

    void onOpen(websocketpp::connection_hdl hdl);
    void onClose(websocketpp::connection_hdl hdl);
    void onMessage(websocketpp::connection_hdl hdl, server_t::message_ptr msg);
    void scheduleTick();
    // Broadcasts one session's state to only that session's own
    // connections - each session ticks and broadcasts independently.
    void broadcastState(int sessionId);

    // Task D3 connection-lifecycle handlers, one per wire message kind
    // reachable before a connection is in a session (see the class
    // comment above for the full state machine).
    void handleLogin(websocketpp::connection_hdl hdl, const std::string& username, const std::string& password);
    void handlePlay(websocketpp::connection_hdl hdl);
    void handleMatch(int seekerIdA, int seekerIdB);
    void handleMatchmakingTimeout(int seekerId);

    // Task D4: reconnect-vs-fresh-login branch, split out of handleLogin()
    // once auth succeeds. True (and fully handled: hdl re-added to the
    // session, seat marked reconnected, timer cancelled, response sent) if
    // `username` owns a currently-disconnected seat in an unfinished game;
    // false means the caller should fall through to the normal
    // logged_in/matchmaking response.
    bool tryReconnect(websocketpp::connection_hdl hdl, const std::string& username, int rating);
    // Starts the 20s auto-resign countdown for sessionId's `color` seat.
    void startDisconnectTimer(int sessionId, char color);
    // Fires when a seat's 20s disconnect window elapses with no reconnect:
    // resigns that color (GameEngine::resign, Task D4) and broadcasts the
    // resulting game-over state.
    void handleDisconnectTimeout(int sessionId, char color);

    // json param is a pre-serialized string, not nlohmann::json - keeps
    // nlohmann out of this header entirely, same convention
    // GameStateSerializer.hpp already established.
    void sendJson(websocketpp::connection_hdl hdl, const std::string& json);
};
#endif
