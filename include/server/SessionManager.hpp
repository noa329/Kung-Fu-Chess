#ifndef SERVER_SESSION_MANAGER_H
#define SERVER_SESSION_MANAGER_H
#include "ConnectionRegistry.hpp"
#include <cstddef>
#include <map>
#include <optional>
#include <vector>

// Task D1: pure connection -> session routing, decoupled from GameSession
// and networking entirely - same extraction pattern ConnectionRegistry
// (A5) already established for testability. Templated on both the
// connection-id type and its comparator: Compare defaults to
// std::less<ConnectionId>, so plain-int tests just work; WebSocketServer
// instantiates this with websocketpp::connection_hdl +
// std::owner_less<connection_hdl> (the same comparator hdlToColor_
// already needs), since connection_hdl - a std::weak_ptr<void> under the
// hood - has no operator< or operator== of its own.
//
// Deliberately does NOT auto-create a new session when every existing one
// is full - see docs/tasks/server-phase-plan.md's D1 entry for the full
// reasoning (spectator support, not yet designed as of this task, should
// be the thing deciding what an overflow connection does - joining/
// watching an existing session, not spinning up an unrelated new one).
// createSession() is a separate, explicit call: WebSocketServer calls it
// once at startup (mirroring A5's exact single-session behavior, so this
// task changes no observable behavior yet); Task D2's matchmaking will be
// the thing that calls it for real once players are actually matched.
//
// Each session's own connection cap is enforced by composing one
// ConnectionRegistry<ConnectionId> per session (reused as-is, not
// reimplemented) rather than a single global one.
template <typename ConnectionId, typename Compare = std::less<ConnectionId>>
class SessionManager {
public:
    explicit SessionManager(size_t capacityPerSession) : capacityPerSession_(capacityPerSession) {}

    // Creates a new, initially empty session. Returns its id (0-based,
    // stable and never reused for the lifetime of this SessionManager -
    // there's no removeSession(), matching ConnectionRegistry's own
    // no-remove()-yet stance; a session's game either finishes normally or
    // this is out of scope for D1).
    int createSession() {
        registries_.emplace_back(capacityPerSession_);
        return static_cast<int>(registries_.size()) - 1;
    }

    // Adds `conn` to `sessionId`. False if that session id doesn't exist
    // or is already at capacity - the caller (WebSocketServer) is
    // responsible for rejecting/closing the connection in that case,
    // exactly like A5's ConnectionRegistry did for a 3rd connection.
    bool tryAdd(int sessionId, const ConnectionId& conn) {
        if (sessionId < 0 || static_cast<size_t>(sessionId) >= registries_.size()) return false;
        if (!registries_[sessionId].tryAdd(conn)) return false;
        connectionToSession_[conn] = sessionId;
        return true;
    }

    // Routing lookup - which session (if any) this connection belongs to.
    std::optional<int> sessionFor(const ConnectionId& conn) const {
        auto it = connectionToSession_.find(conn);
        if (it == connectionToSession_.end()) return std::nullopt;
        return it->second;
    }

    const std::vector<ConnectionId>& connectionsIn(int sessionId) const {
        return registries_[sessionId].connections();
    }

    size_t sessionCount() const { return registries_.size(); }
    size_t occupancy(int sessionId) const { return registries_[sessionId].activeCount(); }

private:
    size_t capacityPerSession_;
    std::vector<ConnectionRegistry<ConnectionId>> registries_;
    std::map<ConnectionId, int, Compare> connectionToSession_;
};
#endif
