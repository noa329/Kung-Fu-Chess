#include "doctest.h"
#include "DisconnectStatusDeserializer.hpp"
#include <nlohmann/json.hpp>

// net_client layer, Task H7: DisconnectStatusDeserializer pulls the
// whiteDisconnectMs/blackDisconnectMs fields back out of the same
// state-broadcast frame GameStateDeserializer.hpp already consumes - see
// its own header comment for why this is a separate parser/type rather
// than folded into GameSnapshot.

using json = nlohmann::json;

namespace {

std::string makeStateBroadcast(json whiteMs, json blackMs) {
    json j;
    j["board"] = json::parse(R"([["wR","."],[".","bK"]])");
    j["cellStates"] = json::parse(R"([["idle","idle"],["idle","idle"]])");
    j["whiteScore"] = 0;
    j["blackScore"] = 0;
    j["whiteMoves"] = json::array();
    j["blackMoves"] = json::array();
    j["gameOver"] = false;
    j["result"] = "";
    j["whiteDisconnectMs"] = whiteMs;
    j["blackDisconnectMs"] = blackMs;
    return j.dump();
}

} // namespace

TEST_CASE("deserialize returns nullopt for malformed JSON") {
    CHECK(!DisconnectStatusDeserializer::deserialize("not json at all").has_value());
}

TEST_CASE("deserialize returns nullopt for well-formed JSON that isn't an object") {
    CHECK(!DisconnectStatusDeserializer::deserialize("[1,2,3]").has_value());
}

TEST_CASE("deserialize returns nullopt for a message with no \"board\" key (e.g. a \"joined\" message)") {
    json j;
    j["type"] = "joined";
    j["color"] = "white";
    CHECK(!DisconnectStatusDeserializer::deserialize(j.dump()).has_value());
}

TEST_CASE("both countdowns null (neither seat disconnected) round-trips to both std::nullopt") {
    auto status = DisconnectStatusDeserializer::deserialize(makeStateBroadcast(nullptr, nullptr));
    REQUIRE(status.has_value());
    CHECK(!status->whiteRemainingMs.has_value());
    CHECK(!status->blackRemainingMs.has_value());
}

TEST_CASE("a real whiteDisconnectMs value round-trips, blackDisconnectMs stays nullopt") {
    auto status = DisconnectStatusDeserializer::deserialize(makeStateBroadcast(12345, nullptr));
    REQUIRE(status.has_value());
    REQUIRE(status->whiteRemainingMs.has_value());
    CHECK(*status->whiteRemainingMs == 12345);
    CHECK(!status->blackRemainingMs.has_value());
}

TEST_CASE("a real blackDisconnectMs value round-trips, whiteDisconnectMs stays nullopt") {
    auto status = DisconnectStatusDeserializer::deserialize(makeStateBroadcast(nullptr, 6789));
    REQUIRE(status.has_value());
    CHECK(!status->whiteRemainingMs.has_value());
    REQUIRE(status->blackRemainingMs.has_value());
    CHECK(*status->blackRemainingMs == 6789);
}

TEST_CASE("both seats disconnected at once round-trips both values") {
    auto status = DisconnectStatusDeserializer::deserialize(makeStateBroadcast(1000, 2000));
    REQUIRE(status.has_value());
    REQUIRE(status->whiteRemainingMs.has_value());
    REQUIRE(status->blackRemainingMs.has_value());
    CHECK(*status->whiteRemainingMs == 1000);
    CHECK(*status->blackRemainingMs == 2000);
}

TEST_CASE("missing keys entirely (not even null) are tolerated, same as absent") {
    json j;
    j["board"] = json::parse(R"([["."]])");
    // whiteDisconnectMs/blackDisconnectMs deliberately omitted.
    auto status = DisconnectStatusDeserializer::deserialize(j.dump());
    REQUIRE(status.has_value());
    CHECK(!status->whiteRemainingMs.has_value());
    CHECK(!status->blackRemainingMs.has_value());
}
