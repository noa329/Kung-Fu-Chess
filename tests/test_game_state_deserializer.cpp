#include "doctest.h"
#include "GameStateDeserializer.hpp"
#include <nlohmann/json.hpp>

// net_client layer: GameStateDeserializer turns a WebSocket state-broadcast
// text frame back into a GameSnapshot - the inverse of
// server/GameStateSerializer.hpp, same style of test (round-trip a known
// JSON string, check the resulting struct field-by-field), just in the
// opposite direction. See the header for which GameSnapshot fields the wire
// protocol never carries and are deliberately defaulted here.

using json = nlohmann::json;

namespace {

std::string makeStateBroadcast() {
    json j;
    j["board"] = json::parse(R"([["wR","."],[".","bK"]])");
    j["cellStates"] = json::parse(R"([["idle","idle"],["idle","idle"]])");
    j["whiteScore"] = 3;
    j["blackScore"] = 0;
    j["whiteMoves"] = json::array({{{"atMs", 1000}, {"color", "w"}, {"notation", "a2a4"}}});
    j["blackMoves"] = json::array();
    j["gameOver"] = false;
    j["result"] = "";
    j["whiteDisconnectMs"] = nullptr;
    j["blackDisconnectMs"] = nullptr;
    return j.dump();
}

} // namespace

TEST_CASE("deserialize returns nullopt for malformed JSON") {
    CHECK(!GameStateDeserializer::deserialize("not json at all").has_value());
}

TEST_CASE("deserialize returns nullopt for well-formed JSON that isn't an object") {
    CHECK(!GameStateDeserializer::deserialize("[1,2,3]").has_value());
    CHECK(!GameStateDeserializer::deserialize("\"just a string\"").has_value());
}

TEST_CASE("deserialize returns nullopt for a typed message with no \"board\" field") {
    // Same shape as "joined"/"logged_in"/G4's discrete pushes - none of
    // which this function is responsible for (see the header comment).
    CHECK(!GameStateDeserializer::deserialize(R"({"type":"joined","color":"white"})").has_value());
    CHECK(!GameStateDeserializer::deserialize(R"({"type":"sound","name":"capture"})").has_value());
}

TEST_CASE("deserialize round-trips board tokens and cellStates exactly") {
    auto snap = GameStateDeserializer::deserialize(makeStateBroadcast());
    REQUIRE(snap.has_value());

    CHECK(snap->boardTokens == std::vector<std::vector<std::string>>{{"wR", "."}, {".", "bK"}});
    CHECK(snap->cellStates == std::vector<std::vector<std::string>>{{"idle", "idle"}, {"idle", "idle"}});
}

TEST_CASE("deserialize round-trips both scores") {
    auto snap = GameStateDeserializer::deserialize(makeStateBroadcast());
    REQUIRE(snap.has_value());

    CHECK(snap->whiteScore == 3);
    CHECK(snap->blackScore == 0);
}

TEST_CASE("deserialize round-trips move history with atMs/color/notation per entry") {
    auto snap = GameStateDeserializer::deserialize(makeStateBroadcast());
    REQUIRE(snap.has_value());

    REQUIRE(snap->whiteMoves.size() == 1);
    CHECK(snap->whiteMoves[0].atMs == 1000);
    CHECK(snap->whiteMoves[0].color == 'w');
    CHECK(snap->whiteMoves[0].notation == "a2a4");
    CHECK(snap->blackMoves.empty());
}

TEST_CASE("deserialize round-trips gameOver=false with an empty result mid-game") {
    auto snap = GameStateDeserializer::deserialize(makeStateBroadcast());
    REQUIRE(snap.has_value());

    CHECK(snap->gameOver == false);
    CHECK(snap->result == "");
}

TEST_CASE("deserialize round-trips gameOver=true with the result string once the game ends") {
    json j = json::parse(makeStateBroadcast());
    j["gameOver"] = true;
    j["result"] = "White Wins";
    auto snap = GameStateDeserializer::deserialize(j.dump());
    REQUIRE(snap.has_value());

    CHECK(snap->gameOver == true);
    CHECK(snap->result == "White Wins");
}

TEST_CASE("deserialize defaults the fields the wire protocol never sends") {
    auto snap = GameStateDeserializer::deserialize(makeStateBroadcast());
    REQUIRE(snap.has_value());

    CHECK(snap->selected == Position{-1, -1});
    CHECK(snap->captureFlashes.empty());
    CHECK(snap->whiteName == "");
    CHECK(snap->blackName == "");

    // Sized to match the board (2x2 in this fixture), every cell defaulted
    // - not left empty/mismatched, since BoardView indexes these by
    // [row][col] alongside boardTokens.
    REQUIRE(snap->moveTargets.size() == 2);
    REQUIRE(snap->moveProgress.size() == 2);
    for (size_t r = 0; r < 2; ++r) {
        REQUIRE(snap->moveTargets[r].size() == 2);
        REQUIRE(snap->moveProgress[r].size() == 2);
        for (size_t c = 0; c < 2; ++c) {
            CHECK(snap->moveTargets[r][c] == Position{-1, -1});
            CHECK(snap->moveProgress[r][c] == 0.0);
        }
    }
}

TEST_CASE("deserialize tolerates missing optional fields instead of throwing") {
    // Only "board" present - everything else should fall back to a sane
    // default rather than the function throwing or returning nullopt.
    json j;
    j["board"] = json::parse(R"([["."]])");
    auto snap = GameStateDeserializer::deserialize(j.dump());
    REQUIRE(snap.has_value());

    CHECK(snap->cellStates.empty());
    CHECK(snap->whiteScore == 0);
    CHECK(snap->blackScore == 0);
    CHECK(snap->whiteMoves.empty());
    CHECK(snap->blackMoves.empty());
    CHECK(snap->gameOver == false);
    CHECK(snap->result == "");
}
