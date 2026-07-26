// Task G2: real-socket automated coverage for WebSocketServer itself -
// onOpen/onClose/onMessage's JSON-type dispatch, broadcastState(), and the
// tick loop's move-resolution sequencing - none of which had any automated
// coverage before this (every other server/ class is pure logic, dual-
// compiled and doctested via the Makefile; WebSocketServer.cpp is the one
// file that needs websocketpp/Asio directly, so it only ever built under
// CMake, and nothing exercised it but manual ws_test_client.py/kungfu_client
// runs). Deliberately real loopback sockets (ServerFixture runs an actual
// WebSocketServer, WsTestClient is an actual websocketpp client), not a
// mock of either - a mock server would test the mock, not this class.
//
// What's intentionally NOT covered here: the real 60s matchmaking timeout
// and the real 20s disconnect-auto-resign timeout actually firing - both
// are already covered as pure decisions (MatchmakingTimeout/
// DisconnectTimeout doctests, dual-compiled) and were both verified against
// real wall-clock timers during D3/D4's own manual verification; waiting
// out 60/20 real seconds per test case here would make this suite
// needlessly slow for no new coverage.
#include "doctest.h"
#include "ServerFixture.hpp"
#include "WsTestClient.hpp"
#include <nlohmann/json.hpp>
#include <chrono>
#include <optional>
#include <string>
#include <thread>

using json = nlohmann::json;

namespace {

json login(WsTestClient& c, const std::string& username, const std::string& password) {
    c.send(json{{"type", "join"}, {"username", username}, {"password", password}}.dump());
    auto msg = c.waitForMessage();
    REQUIRE(msg.has_value());
    return json::parse(*msg);
}

// Repeatedly drains messages (short per-message wait, not a fixed sleep)
// until one satisfies pred or timeoutMs elapses overall - same
// don't-trust-a-fixed-delay reasoning the production code itself uses for
// its real timers (A5/D3/D4).
template <typename Predicate>
std::optional<json> waitForBoardMatching(WsTestClient& c, Predicate pred, int timeoutMs) {
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
        auto msg = c.waitForMessage(100);
        if (!msg) continue;
        try {
            json j = json::parse(*msg);
            if (j.contains("board") && pred(j)) return j;
        } catch (const json::parse_error&) {
            // non-JSON shouldn't happen from this server - ignore defensively
        }
    }
    return std::nullopt;
}

// Drains board-bearing messages currently queued (plus whatever arrives
// shortly after), returning the last one seen - used where a test wants
// "the state right now" rather than "the first state matching X".
// Deliberately bounded by an absolute deadline, NOT "keep going until no
// new message shows up within the per-call wait": a live session's tick
// broadcasts arrive roughly every 16-30ms (WebSocketServer::kTickMs), well
// under any idle-gap threshold that'd be short enough to keep this
// helper fast, so an idle-until-quiet loop against a still-ticking
// session would never actually see a gap and never return.
std::optional<json> latestBoardState(WsTestClient& c, int settleMs) {
    std::this_thread::sleep_for(std::chrono::milliseconds(settleMs));
    std::optional<json> last;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(300);
    while (std::chrono::steady_clock::now() < deadline) {
        auto msg = c.waitForMessage(20);
        if (!msg) break;
        try {
            json j = json::parse(*msg);
            if (j.contains("board")) last = j;
        } catch (const json::parse_error&) {
        }
    }
    return last;
}

} // namespace

TEST_CASE("WebSocketServer: login auto-registers a new username and rejects a wrong password on an existing one") {
    ServerFixture fx;

    WsTestClient c1(fx.port());
    json accepted = login(c1, "G2Alice", "correct-horse");
    CHECK(accepted.value("type", "") == "logged_in");
    CHECK(accepted.value("username", "") == "G2Alice");

    WsTestClient c2(fx.port());
    json rejected = login(c2, "G2Alice", "wrong-password");
    CHECK(rejected.value("type", "") == "join_rejected");
    CHECK(rejected.value("error", "") == "ERROR AUTH_FAILED");
}

TEST_CASE("WebSocketServer: matchmaking pairs two waiting clients and a move resolves and broadcasts to both") {
    ServerFixture fx;

    WsTestClient white(fx.port());
    WsTestClient black(fx.port());
    login(white, "G2MMWhite", "pw");
    login(black, "G2MMBlack", "pw");

    white.send(json{{"type", "play"}}.dump());
    auto searching = white.waitForMessage();
    REQUIRE(searching.has_value());
    CHECK(json::parse(*searching).value("type", "") == "searching");

    black.send(json{{"type", "play"}}.dump());
    auto blackJoined = black.waitForMessage();
    REQUIRE(blackJoined.has_value());
    json blackJson = json::parse(*blackJoined);
    CHECK(blackJson.value("type", "") == "joined");
    CHECK(blackJson.value("color", "") == "black");
    CHECK(blackJson.value("opponent", "") == "G2MMWhite");

    auto whiteJoined = white.waitForMessage();
    REQUIRE(whiteJoined.has_value());
    json whiteJson = json::parse(*whiteJoined);
    CHECK(whiteJson.value("type", "") == "joined");
    CHECK(whiteJson.value("color", "") == "white");
    CHECK(whiteJson.value("opponent", "") == "G2MMBlack");

    // Starting position, before the move below: e2 (rank 2, file e ->
    // board[6][4]) holds a white pawn, e4 (board[4][4]) is empty.
    auto starting = waitForBoardMatching(
        white, [](const json& j) { return j["board"][6][4] == "wP"; }, 2000);
    REQUIRE(starting.has_value());
    CHECK((*starting)["board"][4][4] == ".");

    white.send("WPe2e4");

    // Real travel-time move (~2s simulated, fed from real elapsed ticks -
    // see WebSocketServer::scheduleTick()) - poll rather than assume a
    // fixed resolve time.
    auto resolved = waitForBoardMatching(
        black, [](const json& j) { return j["board"][4][4] == "wP"; }, 4000);
    REQUIRE(resolved.has_value());
    CHECK((*resolved)["board"][6][4] == ".");
}

TEST_CASE("WebSocketServer: room create/join fills seats, a 3rd joiner is a spectator, and a spectator's move is rejected") {
    ServerFixture fx;

    WsTestClient creator(fx.port());
    login(creator, "G2RoomWhite", "pw");
    creator.send(json{{"type", "create_room"}}.dump());
    auto createdMsg = creator.waitForMessage();
    REQUIRE(createdMsg.has_value());
    json created = json::parse(*createdMsg);
    CHECK(created.value("type", "") == "room_created");
    CHECK(created.value("color", "") == "white");
    std::string roomId = created.value("roomId", "");
    REQUIRE_FALSE(roomId.empty());

    WsTestClient joiner(fx.port());
    login(joiner, "G2RoomBlack", "pw");
    joiner.send(json{{"type", "join_room"}, {"roomId", roomId}}.dump());
    auto joinedMsg = joiner.waitForMessage();
    REQUIRE(joinedMsg.has_value());
    json joined = json::parse(*joinedMsg);
    CHECK(joined.value("type", "") == "joined");
    CHECK(joined.value("color", "") == "black");

    WsTestClient spectator(fx.port());
    login(spectator, "G2RoomSpectator", "pw");
    spectator.send(json{{"type", "join_room"}, {"roomId", roomId}}.dump());
    auto spectatedMsg = spectator.waitForMessage();
    REQUIRE(spectatedMsg.has_value());
    json spectated = json::parse(*spectatedMsg);
    CHECK(spectated.value("type", "") == "joined");
    CHECK(spectated.value("color", "") == "spectator");

    // Spectator claims White's own move - GameSession::handleCommand
    // rejects this as ERROR NOT_A_PLAYER (Task E2), but that rejection is
    // only ever logged server-side, never sent back to the sender (see
    // WebSocketServer::onMessage) - so the only observable proof is that
    // the board never actually changes.
    spectator.send("WPe2e4");

    auto state = latestBoardState(creator, 400);
    REQUIRE(state.has_value());
    CHECK((*state)["board"][6][4] == "wP");
    CHECK((*state)["board"][4][4] == ".");
}

TEST_CASE("WebSocketServer: a closed connection starts a disconnect countdown, and reconnecting with the same login clears it") {
    ServerFixture fx;

    auto whiteUsername = std::string("G2DcWhite");
    auto blackUsername = std::string("G2DcBlack");

    // Matchmaking assigns white to whichever client's play() reaches the
    // server first (confirmed by the matchmaking test case above) - white
    // plays first here and stays connected for the whole test, observing
    // broadcasts; black plays second and is the one disconnected/
    // reconnected below.
    WsTestClient white(fx.port());
    login(white, whiteUsername, "pw");
    white.send(json{{"type", "play"}}.dump());
    white.waitForMessage(); // "searching"

    {
        WsTestClient black(fx.port());
        login(black, blackUsername, "pw");
        black.send(json{{"type", "play"}}.dump());
        black.waitForMessage(); // "joined" for black
        white.waitForMessage(); // "joined" for white
        // black goes out of scope here - its destructor tears the socket
        // down, which is what should drive the server's onClose().
    }

    // blackDisconnectMs is always present in the payload (null when not
    // applicable, see GameStateSerializer) - non-null is the signal a
    // disconnect was actually detected and the 20s countdown started.
    auto disconnected = waitForBoardMatching(
        white, [](const json& j) { return !j["blackDisconnectMs"].is_null(); }, 3000);
    REQUIRE(disconnected.has_value());

    WsTestClient reconnecting(fx.port());
    reconnecting.send(json{{"type", "join"}, {"username", blackUsername}, {"password", "pw"}}.dump());
    auto reconnectedMsg = reconnecting.waitForMessage();
    REQUIRE(reconnectedMsg.has_value());
    json reconnected = json::parse(*reconnectedMsg);
    CHECK(reconnected.value("type", "") == "reconnected");
    CHECK(reconnected.value("color", "") == "black");
    CHECK(reconnected.value("opponent", "") == whiteUsername);

    auto cleared = waitForBoardMatching(
        white, [](const json& j) { return j["blackDisconnectMs"].is_null(); }, 2000);
    REQUIRE(cleared.has_value());
}

TEST_CASE("WebSocketServer: resign ends the game and cancels a pending disconnect timer for the other seat") {
    ServerFixture fx;

    auto whiteUsername = std::string("G2ResignWhite");
    auto blackUsername = std::string("G2ResignBlack");

    WsTestClient white(fx.port());
    login(white, whiteUsername, "pw");
    white.send(json{{"type", "play"}}.dump());
    white.waitForMessage(); // "searching"

    {
        WsTestClient black(fx.port());
        login(black, blackUsername, "pw");
        black.send(json{{"type", "play"}}.dump());
        black.waitForMessage(); // "joined" for black
        white.waitForMessage(); // "joined" for white
        // black goes out of scope here - its destructor tears the socket
        // down, giving black's seat a live 20s auto-resign timer by the
        // time white resigns below. This is what proves the resign path
        // actually cancels a pending timer (Task G3's resolved open
        // question) instead of leaving it to fire later into an
        // already-decided game.
    }

    auto disconnected = waitForBoardMatching(
        white, [](const json& j) { return !j["blackDisconnectMs"].is_null(); }, 3000);
    REQUIRE(disconnected.has_value());

    white.send("resign");

    auto resigned = waitForBoardMatching(
        white, [](const json& j) { return j["gameOver"] == true; }, 2000);
    REQUIRE(resigned.has_value());
    CHECK((*resigned)["result"] == "Black Wins");
    // The pending disconnect timer for black's seat must be cancelled by
    // the resign, not just superseded by gameOver - a stale timer left
    // running would otherwise still be free to fire ~20s later (harmless
    // since GameEngine::resign() is idempotent, but the broadcast would
    // keep showing a live countdown on an already-decided game until it
    // does).
    CHECK((*resigned)["blackDisconnectMs"].is_null());
}

TEST_CASE("WebSocketServer: a spectator's resign is rejected without ending the game") {
    ServerFixture fx;

    WsTestClient creator(fx.port());
    login(creator, "G2ResignRoomWhite", "pw");
    creator.send(json{{"type", "create_room"}}.dump());
    auto createdMsg = creator.waitForMessage();
    REQUIRE(createdMsg.has_value());
    std::string roomId = json::parse(*createdMsg).value("roomId", "");
    REQUIRE_FALSE(roomId.empty());

    WsTestClient joiner(fx.port());
    login(joiner, "G2ResignRoomBlack", "pw");
    joiner.send(json{{"type", "join_room"}, {"roomId", roomId}}.dump());
    joiner.waitForMessage(); // "joined" as black

    WsTestClient spectator(fx.port());
    login(spectator, "G2ResignRoomSpectator", "pw");
    spectator.send(json{{"type", "join_room"}, {"roomId", roomId}}.dump());
    spectator.waitForMessage(); // "joined" as spectator

    spectator.send("resign");

    auto state = latestBoardState(creator, 400);
    REQUIRE(state.has_value());
    CHECK((*state)["gameOver"] == false);
}
