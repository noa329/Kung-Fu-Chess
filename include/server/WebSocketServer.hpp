#ifndef SERVER_WEBSOCKET_SERVER_H
#define SERVER_WEBSOCKET_SERVER_H
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#include <asio.hpp>
#include "GameSession.hpp"
#include "ConnectionRegistry.hpp"
#include "Logger.hpp"
#include <cstdint>
#include <memory>
#include <chrono>
#include <fstream>
#include <map>

// Wires GameCommandParser (A2) + GameSession (A3) + GameStateSerializer
// (A4) together over a real WebSocket connection: exactly one hardcoded
// GameSession, up to 2 connections (a 3rd is rejected outright).
// Generalizing to N concurrent sessions (SessionManager) is Task D1, not
// here.
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

    explicit WebSocketServer(uint16_t port);

    // Blocks, running the asio event loop (accept + message handling +
    // the periodic engine tick) until the process is killed.
    void run();

private:
    static constexpr size_t kMaxConnections = 2;
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
    // logFile_ must outlive logger_, and logger_ must outlive session_'s
    // use of it (attachLogger() just stores a pointer, doesn't extend
    // lifetime), hence this exact declaration order.
    std::ofstream logFile_;
    Logger logger_;
    GameSession session_;
    ConnectionRegistry<websocketpp::connection_hdl> registry_;
    std::unique_ptr<asio::steady_timer> tickTimer_;
    std::chrono::steady_clock::time_point lastTickTime_;
    // Task B2: which color each connection was assigned on join. Populated
    // by tryHandleJoin(), not onOpen() - color assignment is driven by join
    // *message* order, not connection-accept order, per GameSession::
    // handleJoin(). connection_hdl is a std::weak_ptr<void> with no
    // operator<, so std::owner_less is the standard websocketpp-documented
    // way to use it as a map key.
    std::map<websocketpp::connection_hdl, char, std::owner_less<websocketpp::connection_hdl>> hdlToColor_;

    void onOpen(websocketpp::connection_hdl hdl);
    void onMessage(websocketpp::connection_hdl hdl, server_t::message_ptr msg);
    void scheduleTick();
    void broadcastState();
    // Returns true if `payload` was a well-formed join message and has
    // already been fully handled (color assigned + response(s) sent) - the
    // caller should not also try to parse it as a game command. Returns
    // false for anything that isn't a join message, including plain
    // game-command strings like "WQe2e5" (which aren't valid JSON at all),
    // so onMessage's existing GameCommandParser path is completely
    // unaffected.
    bool tryHandleJoin(websocketpp::connection_hdl hdl, const std::string& payload);
    // json param is a pre-serialized string, not nlohmann::json - keeps
    // nlohmann out of this header entirely, same convention
    // GameStateSerializer.hpp already established.
    void sendJson(websocketpp::connection_hdl hdl, const std::string& json);
};
#endif
