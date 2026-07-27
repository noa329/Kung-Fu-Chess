#include "doctest.h"
#include "GameStateSerializer.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>

// server layer: GameStateSerializer turns a GameSnapshot into the JSON
// string broadcast to clients. Deliberately a REDUCED subset - additive
// alongside GameSnapshot, not a replacement for it (the local graphics
// renderer keeps reading GameSnapshot directly at 60fps; this is only for
// the network wire). Included: board tokens, cellStates, scores, move
// history, gameOver/result, and (Task I1) a sparse activeMoves list for
// exactly the cells with cellStates=="move" - see
// docs/tasks/wire-protocol-move-progress-plan.md. Excluded, on purpose:
// selected (per-connection UI-gesture state, not shared game state) and
// captureFlashes (render-loop-only decoration - the text-only shell
// client, Phase B, has nothing to show a capture flash with; revisit only
// if/when it turns out to matter for the graphics binary too, a separate
// unscoped future task).

using json = nlohmann::json;

namespace {

GameSnapshot makeTestSnapshot() {
    GameSnapshot snap{};
    snap.boardTokens = {{"wR", "."}, {".", "bK"}};
    snap.cellStates = {{"idle", "idle"}, {"idle", "idle"}};
    snap.whiteScore = 3;
    snap.blackScore = 0;
    snap.whiteMoves = {{1000, 'w', "a2a4"}};
    snap.blackMoves = {};
    snap.gameOver = false;
    snap.result = "";
    return snap;
}

} // namespace

TEST_CASE("serialize includes board tokens and cellStates exactly as given") {
    auto snap = makeTestSnapshot();
    json j = json::parse(GameStateSerializer::serialize(snap));

    // json::parse(...) on a string literal, not the {{...}} initializer-list
    // constructor: nlohmann::json's braced-init is ambiguous between "array
    // of 2-element arrays" and "object" and picks object here, which isn't
    // what a nested board array should compare against.
    CHECK(j.at("board") == json::parse(R"([["wR","."],[".","bK"]])"));
    CHECK(j.at("cellStates") == json::parse(R"([["idle","idle"],["idle","idle"]])"));
}

TEST_CASE("serialize includes both scores") {
    auto snap = makeTestSnapshot();
    json j = json::parse(GameStateSerializer::serialize(snap));

    CHECK(j.at("whiteScore") == 3);
    CHECK(j.at("blackScore") == 0);
}

TEST_CASE("serialize includes move history with atMs/color/notation per entry") {
    auto snap = makeTestSnapshot();
    json j = json::parse(GameStateSerializer::serialize(snap));

    REQUIRE(j.at("whiteMoves").size() == 1);
    CHECK(j.at("whiteMoves")[0].at("atMs") == 1000);
    CHECK(j.at("whiteMoves")[0].at("color") == "w");
    CHECK(j.at("whiteMoves")[0].at("notation") == "a2a4");
    CHECK(j.at("blackMoves").empty());
}

TEST_CASE("serialize reports gameOver=false with an empty result mid-game") {
    auto snap = makeTestSnapshot();
    json j = json::parse(GameStateSerializer::serialize(snap));

    CHECK(j.at("gameOver") == false);
    CHECK(j.at("result") == "");
}

TEST_CASE("serialize reports gameOver=true with the result string once the game ends") {
    auto snap = makeTestSnapshot();
    snap.gameOver = true;
    snap.result = "White Wins";
    json j = json::parse(GameStateSerializer::serialize(snap));

    CHECK(j.at("gameOver") == true);
    CHECK(j.at("result") == "White Wins");
}

TEST_CASE("serialize emits exactly the approved field set, nothing more") {
    auto snap = makeTestSnapshot();
    json j = json::parse(GameStateSerializer::serialize(snap));

    std::vector<std::string> keys;
    for (auto it = j.begin(); it != j.end(); ++it) keys.push_back(it.key());
    std::sort(keys.begin(), keys.end());

    std::vector<std::string> expected = {
        "activeMoves", "blackDisconnectMs", "blackMoves", "blackScore", "board",
        "cellStates", "gameOver", "result", "whiteDisconnectMs", "whiteMoves",
        "whiteScore"
    };
    std::sort(expected.begin(), expected.end());

    CHECK(keys == expected);
}

// Task I1: sparse activeMoves - only cells with cellStates=="move" ever
// contribute an entry (see GameStateSerializer.hpp's own comment for why
// this is sparse rather than a dense per-cell grid: bandwidth at the 60Hz
// broadcast tick, worked out in docs/tasks/wire-protocol-move-progress-plan.md).

TEST_CASE("serialize's activeMoves is empty when nothing is mid-move") {
    auto snap = makeTestSnapshot(); // all cellStates "idle"
    json j = json::parse(GameStateSerializer::serialize(snap));

    CHECK(j.at("activeMoves").empty());
}

TEST_CASE("serialize's activeMoves reports from/to/progress for exactly the mid-move cell") {
    GameSnapshot snap{};
    snap.boardTokens = {{"wR", "."}, {".", "bK"}};
    snap.cellStates = {{"move", "idle"}, {"idle", "idle"}};
    snap.moveTargets = {{Position{1, 1}, Position{}}, {Position{}, Position{}}};
    snap.moveProgress = {{0.42, 0.0}, {0.0, 0.0}};
    json j = json::parse(GameStateSerializer::serialize(snap));

    REQUIRE(j.at("activeMoves").size() == 1);
    const auto& entry = j.at("activeMoves")[0];
    CHECK(entry.at("from").at("row") == 0);
    CHECK(entry.at("from").at("col") == 0);
    CHECK(entry.at("to").at("row") == 1);
    CHECK(entry.at("to").at("col") == 1);
    CHECK(entry.at("progress") == doctest::Approx(0.42));
}

// Task D4: per-color disconnect countdown, present only while that color
// is actually mid-countdown - see GameStateSerializer.hpp's
// DisconnectStatus comment for why this isn't part of GameSnapshot
// itself (connection-layer state, not game state).

TEST_CASE("serialize reports null disconnect fields when no one is disconnected") {
    auto snap = makeTestSnapshot();
    json j = json::parse(GameStateSerializer::serialize(snap)); // default DisconnectStatus - both nullopt

    CHECK(j.at("whiteDisconnectMs").is_null());
    CHECK(j.at("blackDisconnectMs").is_null());
}

TEST_CASE("serialize reports the remaining ms for whichever color is disconnected") {
    auto snap = makeTestSnapshot();
    GameStateSerializer::DisconnectStatus disconnect;
    disconnect.blackRemainingMs = 12345;
    json j = json::parse(GameStateSerializer::serialize(snap, disconnect));

    CHECK(j.at("whiteDisconnectMs").is_null());
    CHECK(j.at("blackDisconnectMs") == 12345);
}
