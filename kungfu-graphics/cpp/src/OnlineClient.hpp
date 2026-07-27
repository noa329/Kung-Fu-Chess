#pragma once
#include "GameEngine.hpp"
#include "Events.hpp"
#include "DisconnectStatusDeserializer.hpp"
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

// docs/tasks/graphics-networked-client-plan.md, Task H3b: owns the real
// websocketpp connection to kungfu_server for the graphics binary's Online
// Play flow. Deliberately graphics-build-only (kungfu-graphics/cpp/src/,
// not the shared net_client layer under include/net_client - see Task H2)
// since it needs websocketpp/Asio, only available to this CMake target
// (Task H1); OnlineMenuView (renderer layer) knows nothing about this
// class at all, same separation Controller/GameEngine already have from
// BoardView/HudView.
//
// Pimpl'd (same pattern as audio/SoundManager.hpp) so this header - and
// anything that includes it, i.e. main.cpp - never has to see websocketpp/
// nlohmann types directly.
//
// Threading model (see the plan's own section for the full reasoning):
// a single background thread runs the websocketpp io_context for the
// whole lifetime of the connection. Every poll*() method is safe to call
// every frame from the render-loop thread - each one takes a short-lived
// mutex, consumes (clears) whatever's pending, and returns immediately
// rather than blocking, so the OpenCV window keeps pumping frames during
// a real login/matchmaking round trip instead of freezing.
//
// Task H3b's own scope stopped at "successfully joined" (a match or room
// join succeeds) - the connection deliberately stays open past that point
// per the plan's confirmed decision. Task H5 extends this class to also
// capture the periodic state-tick broadcasts via GameStateDeserializer
// (net_client layer, Task H2) - see pollGameState() below - reusing this
// same connection, not opening a second one. Task H6 further extends it to
// queue G4's discrete sound/lifecycle pushes via NetworkEventParser (same
// net_client layer) - see pollEvents() below. Task H7 adds one more
// coalesced side-channel, the per-color disconnect countdown, via
// DisconnectStatusDeserializer (same net_client layer again) - see
// pollDisconnectStatus() below.
class OnlineClient {
public:
    // Mirrors client/cli/main.cpp's own LoginState fields/meaning exactly,
    // but consumed via non-blocking poll (see pollResponse() below)
    // instead of a condition_variable wait.
    struct ServerResponse {
        bool ok = false;
        std::string error;
        std::string color;         // "white" | "black" | "spectator", meaningful only when ok
        std::string opponent;
        std::string whiteUsername; // meaningful only when color == "spectator"
        std::string blackUsername;
        bool reconnected = false;  // true only for a "reconnected" login response (Task D4)
        std::string roomId;        // meaningful only for a "room_created" response
    };

    OnlineClient();
    ~OnlineClient();

    OnlineClient(const OnlineClient&) = delete;
    OnlineClient& operator=(const OnlineClient&) = delete;

    // Starts the background thread and opens the connection. Non-blocking -
    // poll the outcome via pollConnectionOpened().
    void connect(const std::string& uri);

    // Wire actions - each expects exactly one pollResponse() to eventually
    // report the outcome, mirroring the fact that this flow (login, then
    // exactly one of play/create_room/join_room) only ever has one request
    // in flight at a time, same simplifying assumption client/cli's own
    // LoginState already makes.
    void sendLogin(const std::string& username, const std::string& password);
    void sendPlay();
    void sendCreateRoom();
    void sendJoinRoom(const std::string& roomId);

    // Task H4: sends a raw game-command string ("WQe2e5"/"JWPe2"/"resign")
    // directly, not wrapped in JSON - matches GameCommandParser's wire
    // grammar exactly (include/server/GameCommandParser.hpp), same as
    // client/cli's own gameplay loop. Only meaningful once inside a session
    // (after a successful match/room-join); sending this before that just
    // reaches WebSocketServer::onMessage()'s "not yet in a session" branch,
    // which silently drops anything that isn't valid login/matchmaking/
    // room JSON - no response is expected either way, unlike sendLogin()/
    // sendPlay()/etc above.
    void sendCommand(const std::string& raw);

    // Closes the connection and joins the background thread. Safe to call
    // even if connect() was never called, or was already disconnected -
    // idempotent past the first real call.
    void disconnect();

    // std::nullopt until the initial connection attempt resolves; then
    // true (opened) or false (failed) exactly once - consumed on read.
    std::optional<bool> pollConnectionOpened();

    // Consumes and clears the response to whichever request was last sent -
    // std::nullopt if nothing new has arrived since the last call.
    std::optional<ServerResponse> pollResponse();

    // Task H5: consumes the latest periodic state-tick broadcast, if a new
    // one arrived since the last poll - std::nullopt otherwise (including
    // before any session exists, or between ticks). Unlike pollResponse()'s
    // "exactly one request in flight" assumption, these arrive continuously
    // once in a session - this is a coalesced "latest value" slot, not a
    // queue (see the plan's threading-model section), so the caller should
    // cache whatever it last got and keep rendering that every frame, not
    // treat a nullopt poll as "nothing to show."
    std::optional<GameSnapshot> pollGameState();

    // Task H6: drains and returns every discrete sound/lifecycle event that
    // arrived since the last poll, in arrival order - a real queue, unlike
    // pollGameState()'s coalesced "latest value" slot, because every entry
    // here must fire exactly once (a dropped capture-sound push, or a
    // dropped game-end push, is a real bug, not a stale value that's fine
    // to skip - see the plan's threading-model section). Empty if nothing
    // arrived.
    std::vector<std::variant<SoundEvent, GameLifecycleEvent>> pollEvents();

    // Task H7: consumes the latest per-color disconnect countdown parsed
    // off the same periodic state-tick broadcast pollGameState() consumes
    // (see DisconnectStatusDeserializer.hpp) - coalesced "latest value"
    // semantics identical to pollGameState()'s own (not a queue, unlike
    // pollEvents() above): the caller should cache whatever it last got and
    // keep passing it to HudView::compose() every frame. Both fields go
    // back to std::nullopt on the very next tick once a disconnected seat
    // reconnects, since the server simply stops including that field.
    std::optional<DisconnectStatus> pollDisconnectStatus();

    // Whether the socket is currently open - independent of
    // pollConnectionOpened()'s one-shot initial result, this can flip
    // false later too (a server-initiated close).
    bool isConnected() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
