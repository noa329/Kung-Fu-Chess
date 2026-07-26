#ifndef NET_CLIENT_ONLINE_FLOW_STATE_H
#define NET_CLIENT_ONLINE_FLOW_STATE_H
#include <string>

// docs/tasks/graphics-networked-client-plan.md, Task H3b: the pure "what
// screen comes next" decision for the Online Play login/menu/room flow,
// factored out of kungfu-graphics/cpp/src/main.cpp's runOnlineGame() so
// it's unit-testable without a real window or socket - same "drive it via
// its public API instead of a real terminal/window" reasoning
// TextTestRunner already established for the text protocol. Screens are
// plain strings ("connecting" | "login" | "menu" | "room_id_entry" |
// "waiting" | "connection_failed" | "status_connected"), matching this
// project's strings-not-enums convention for state identifiers.
namespace OnlineFlowState {

// What screen to move to once a pending request resolves - mirrors
// exactly what runOnlineGame()'s "waiting" screen does with each server
// response, factored out so "a rejected login/room-join bounces back to
// the right screen, an accepted one advances" is testable without a real
// server round trip. `pendingAction` is whichever wire request is in
// flight ("login" | "play" | "create_room" | "join_room"); `reconnected`
// is only meaningful when pendingAction == "login" and responseOk is true
// (Task D4's reconnect path skips matchmaking/room selection entirely).
std::string nextScreenAfterResponse(const std::string& pendingAction, bool responseOk, bool reconnected);

}
#endif
