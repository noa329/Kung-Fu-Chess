#include "doctest.h"
#include "NetworkEventParser.hpp"

// net_client layer: NetworkEventParser turns Task G4's discrete
// "type":"sound"/"type":"lifecycle" push messages back into the exact same
// SoundEvent/GameLifecycleEvent types GameEngine's own EventBus publishes
// locally - see the header for why that reuse matters (Task H6 feeds these
// straight into SoundManager/HudView, same as the local subscribers do).

TEST_CASE("parse returns nullopt for malformed JSON") {
    CHECK(!NetworkEventParser::parse("not json at all").has_value());
}

TEST_CASE("parse returns nullopt for well-formed JSON that isn't an object") {
    CHECK(!NetworkEventParser::parse("[1,2,3]").has_value());
}

TEST_CASE("parse returns nullopt for a state-broadcast frame (has \"board\", no \"type\")") {
    CHECK(!NetworkEventParser::parse(R"({"board":[["."]],"gameOver":false})").has_value());
}

TEST_CASE("parse returns nullopt for a recognized-but-unrelated \"type\"") {
    // join/joined/room_created/etc - Task H3b's concern, not this parser's.
    CHECK(!NetworkEventParser::parse(R"({"type":"joined","color":"white"})").has_value());
}

TEST_CASE("parse decodes a sound event") {
    auto result = NetworkEventParser::parse(R"({"type":"sound","name":"capture"})");
    REQUIRE(result.has_value());
    REQUIRE(std::holds_alternative<SoundEvent>(*result));
    CHECK(std::get<SoundEvent>(*result).name == "capture");
}

TEST_CASE("parse decodes a lifecycle-start event with no result field") {
    auto result = NetworkEventParser::parse(R"({"type":"lifecycle","phase":"start"})");
    REQUIRE(result.has_value());
    REQUIRE(std::holds_alternative<GameLifecycleEvent>(*result));
    const auto& e = std::get<GameLifecycleEvent>(*result);
    CHECK(e.phase == "start");
    CHECK(e.result == "");
}

TEST_CASE("parse decodes a lifecycle-end event with its result string") {
    auto result = NetworkEventParser::parse(R"({"type":"lifecycle","phase":"end","result":"White Wins"})");
    REQUIRE(result.has_value());
    REQUIRE(std::holds_alternative<GameLifecycleEvent>(*result));
    const auto& e = std::get<GameLifecycleEvent>(*result);
    CHECK(e.phase == "end");
    CHECK(e.result == "White Wins");
}
