#include "doctest.h"
#include "GameStateSerializer.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>

// server layer: GameStateSerializer turns a GameSnapshot into the JSON
// string broadcast to clients. Deliberately a REDUCED subset - additive
// alongside GameSnapshot, not a replacement for it (the local graphics
// renderer keeps reading GameSnapshot directly at 60fps; this is only for
// the network wire). Included: board tokens, cellStates, scores, move
// history, gameOver/result. Excluded, on purpose: moveTargets/moveProgress/
// selected (render-loop-only, meaningless without a 60fps client tick) and
// captureFlashes (same category - the text-only shell client, Phase B, has
// nothing to show a capture flash with; revisit only if/when the graphics
// binary gets network support, a separate unscoped future task).

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
        "blackMoves", "blackScore", "board", "cellStates",
        "gameOver", "result", "whiteMoves", "whiteScore"
    };
    std::sort(expected.begin(), expected.end());

    CHECK(keys == expected);
}
