#include "OnlineFlowState.hpp"

namespace OnlineFlowState {

std::string nextScreenAfterResponse(const std::string& pendingAction, bool responseOk, bool reconnected) {
    if (!responseOk) {
        if (pendingAction == "login") return "login";
        if (pendingAction == "join_room") return "room_id_entry";
        return "menu"; // "play" / "create_room" rejection - back to the menu
    }
    if (pendingAction == "login") {
        return reconnected ? "status_connected" : "menu";
    }
    // "play" / "create_room" / "join_room" all succeed the same way.
    return "status_connected";
}

}
