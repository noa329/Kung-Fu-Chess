#ifndef SERVER_WEBSOCKET_SERVER_H
#define SERVER_WEBSOCKET_SERVER_H
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#include <asio.hpp>
#include "GameSession.hpp"
#include "SessionManager.hpp"
#include "Logger.hpp"
#include "Database.hpp"
#include "UserRepository.hpp"
#include "AuthService.hpp"
#include <cstdint>
#include <memory>
#include <chrono>
#include <fstream>
#include <map>
#include <vector>

// Wires GameCommandParser (A2) + GameSession (A3) + GameStateSerializer
// (A4) together over a real WebSocket connection.
//
// Task D1: generalized from A5's single hardcoded GameSession to N
// concurrent ones, routed via SessionManager. Deliberately no observable
// behavior change yet - exactly one session is created at startup
// (createSession(), called once from the constructor, same as A5), and
// onOpen() only ever tries to add a new connection to sessions that
// already exist; it does NOT create a new one when every existing session
// is full, so a 3rd connection is still rejected outright, exactly like
// A5. Auto-creating a session on overflow was considered and deliberately
// rejected for this task: spectator support (not yet designed as of D1)
// should be the thing deciding what an overflow connection does - joining
// or watching an existing session, not spinning up an unrelated new one.
// Task D2's matchmaking is what will call createSession() for real once
// players are matched.
//
// Task B2: also detects and handles `{"type":"join","username":"..."}`
// control messages ahead of GameCommandParser's plain-string game-command
// grammar - see tryHandleJoin(). Wire response shapes (all JSON, this
// class's own design decision, not specified by the deck beyond the
// "you are White"/"opponent connected: <name>" wording):
//   -> joining client on success:  {"type":"joined","color":"white"|"black","username":"..."}
//   -> joining client on reject:   {"type":"join_rejected","error":"ERROR ..."}
//   -> existing opponent (if any): {"type":"opponent_joined","username":"..."}
//
// Task C3: the join message now also carries "password" (auto-register on
// a never-seen-before username, per the resolved C4 open question - see
// AuthService). Owns the real Database/UserRepository/AuthService for this
// process - data/kungfu_chess.db, created by server/main.cpp before this
// class is constructed (SQLite can create the .db file itself but not a
// missing parent directory). These stay process-wide (one Database, one
// UserRepository, one AuthService), not per-session - only the game state
// itself (GameSession) is what Task D1 multiplies.
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
    // reference to it, and both must exist before any GameSession's
    // attachAuthService() call.
    Database database_;
    UserRepository userRepository_;
    AuthService authService_;
    // Task D1: connection -> session routing (pure, see SessionManager.hpp)
    // plus the actual GameSession instances, parallel-indexed by
    // SessionManager's session ids. unique_ptr so pointers/references into
    // sessions_ stay stable as the vector grows (GameSession itself is
    // neither copyable nor cheaply movable, and doesn't need to be).
    SessionManager<websocketpp::connection_hdl, hdl_compare> sessionManager_;
    std::vector<std::unique_ptr<GameSession>> sessions_;
    std::unique_ptr<asio::steady_timer> tickTimer_;
    std::chrono::steady_clock::time_point lastTickTime_;
    // Task B2: which color each connection was assigned on join. Still a
    // single flat map, not per-session - a connection_hdl is already a
    // globally unique identity regardless of which session it's in, so
    // there's nothing to gain from scoping this map itself; only the
    // opponent-lookup loop (tryHandleJoin) needs to know which *other*
    // hdls share this connection's session, which it gets from
    // sessionManager_.connectionsIn() instead. Populated by tryHandleJoin(),
    // not onOpen() - color assignment is driven by join *message* order,
    // not connection-accept order, per GameSession::handleJoin().
    // connection_hdl is a std::weak_ptr<void> with no operator<, so
    // std::owner_less is the standard websocketpp-documented way to use it
    // as a map key.
    std::map<websocketpp::connection_hdl, char, hdl_compare> hdlToColor_;

    // Creates a new session: SessionManager bookkeeping plus the actual
    // GameSession (logger/authService attached, standard board loaded).
    // Task D1 only ever calls this once, from the constructor - see the
    // class comment above for why it doesn't auto-create more.
    int createSession();

    void onOpen(websocketpp::connection_hdl hdl);
    void onMessage(websocketpp::connection_hdl hdl, server_t::message_ptr msg);
    void scheduleTick();
    // Broadcasts one session's state to only that session's own
    // connections - each session ticks and broadcasts independently.
    void broadcastState(int sessionId);
    // Returns true if `payload` was a well-formed join message and has
    // already been fully handled (color assigned + response(s) sent) - the
    // caller should not also try to parse it as a game command. Returns
    // false for anything that isn't a join message, including plain
    // game-command strings like "WQe2e5" (which aren't valid JSON at all),
    // so onMessage's existing GameCommandParser path is completely
    // unaffected.
    bool tryHandleJoin(websocketpp::connection_hdl hdl, int sessionId, const std::string& payload);
    // json param is a pre-serialized string, not nlohmann::json - keeps
    // nlohmann out of this header entirely, same convention
    // GameStateSerializer.hpp already established.
    void sendJson(websocketpp::connection_hdl hdl, const std::string& json);
};
#endif
