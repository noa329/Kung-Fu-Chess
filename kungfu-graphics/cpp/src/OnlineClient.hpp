#pragma once
#include <memory>
#include <optional>
#include <string>

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
// Task H3b's own scope stops at "successfully joined" (a match or room
// join succeeds) - the connection deliberately stays open past that point
// per the plan's confirmed decision, but nothing here yet captures the
// periodic state-tick broadcasts or G4's discrete sound/lifecycle pushes
// that start arriving once a session exists; they're silently ignored by
// this class's message handler for now. Task H5/H6 extend this class to
// actually capture those via GameStateDeserializer/NetworkEventParser
// (net_client layer, Task H2) - reusing this same connection, not opening
// a second one.
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

    // Whether the socket is currently open - independent of
    // pollConnectionOpened()'s one-shot initial result, this can flip
    // false later too (a server-initiated close).
    bool isConnected() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
