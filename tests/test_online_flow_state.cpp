#include "doctest.h"
#include "OnlineFlowState.hpp"

// net_client layer: the pure "which screen comes next" decision behind
// runOnlineGame()'s "waiting" screen - see the header for why this is
// split out of main.cpp itself.

TEST_CASE("a rejected login bounces back to the login screen") {
    CHECK(OnlineFlowState::nextScreenAfterResponse("login", false, false) == "login");
}

TEST_CASE("a rejected join_room bounces back to the room ID prompt") {
    CHECK(OnlineFlowState::nextScreenAfterResponse("join_room", false, false) == "room_id_entry");
}

TEST_CASE("a rejected play or create_room bounces back to the menu") {
    CHECK(OnlineFlowState::nextScreenAfterResponse("play", false, false) == "menu");
    CHECK(OnlineFlowState::nextScreenAfterResponse("create_room", false, false) == "menu");
}

TEST_CASE("a fresh accepted login advances to the menu") {
    CHECK(OnlineFlowState::nextScreenAfterResponse("login", true, false) == "menu");
}

TEST_CASE("a reconnect-accepted login skips the menu and goes straight to status") {
    CHECK(OnlineFlowState::nextScreenAfterResponse("login", true, true) == "status_connected");
}

TEST_CASE("an accepted play/create_room/join_room all land on the status screen") {
    CHECK(OnlineFlowState::nextScreenAfterResponse("play", true, false) == "status_connected");
    CHECK(OnlineFlowState::nextScreenAfterResponse("create_room", true, false) == "status_connected");
    CHECK(OnlineFlowState::nextScreenAfterResponse("join_room", true, false) == "status_connected");
}
