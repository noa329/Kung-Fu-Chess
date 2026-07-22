#ifndef SERVER_ROOM_REGISTRY_H
#define SERVER_ROOM_REGISTRY_H
#include <functional>
#include <map>
#include <optional>
#include <string>

// Task E1: room ID <-> session ID discovery, pure and socket-independent -
// same "pure decision, composition root does the actual wiring" split
// MatchmakingQueue (D2) and SessionManager (D1) already established.
// RoomRegistry never creates a GameSession itself: WebSocketServer calls
// createSession() first (exactly like handleMatch() already does), then
// registers the result here to get back a room ID. The role a joiner
// receives (White/Black/spectator) is entirely GameSession::join()'s
// decision, not this class's - see GameSession.hpp's class comment for
// why that logic lives in exactly one place shared by both the
// matchmaking path and a room's join-by-ID path.
namespace RoomIdGenerator {
// 6-character code from a 32-character alphanumeric charset that excludes
// visually ambiguous characters (0/O, 1/I) - meant to be read off one
// player's screen and typed by another. Single case (uppercase-only) so
// room ID lookups stay a plain exact-string match, no case-folding needed
// anywhere else in this codebase.
std::string generate();
} // namespace RoomIdGenerator

class RoomRegistry {
public:
    // idGenerator is injectable (defaults to the real RoomIdGenerator::
    // generate) so tests can supply a deterministic/fixed sequence instead
    // of relying on true randomness - same testability-via-injection
    // reasoning SessionManager's Compare template param exists for,
    // applied to a function dependency instead of a type one.
    explicit RoomRegistry(std::function<std::string()> idGenerator = RoomIdGenerator::generate)
        : idGenerator_(std::move(idGenerator)) {}

    // Generates a fresh id for `sessionId`, retrying on the (astronomically
    // unlikely, but not undefined-behavior-unlikely) case that idGenerator_
    // produces an id already in use.
    std::string createRoom(int sessionId) {
        std::string id;
        do {
            id = idGenerator_();
        } while (roomToSession_.count(id));
        roomToSession_[id] = sessionId;
        return id;
    }

    std::optional<int> sessionForRoom(const std::string& roomId) const {
        auto it = roomToSession_.find(roomId);
        if (it == roomToSession_.end()) return std::nullopt;
        return it->second;
    }

private:
    std::function<std::string()> idGenerator_;
    std::map<std::string, int> roomToSession_;
};
#endif
