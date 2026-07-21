#ifndef SERVER_CONNECTION_REGISTRY_H
#define SERVER_CONNECTION_REGISTRY_H
#include <cstddef>
#include <vector>

// Pure, socket-independent capacity tracking - the part of
// WebSocketServer's "accept or reject a new connection" decision that's
// actually testable without real sockets. Templated on the handle type
// so tests can use plain ints; WebSocketServer instantiates it with
// websocketpp::connection_hdl.
//
// No remove() yet - deliberately. Task A5 only ever adds connections at
// accept time and holds them for the session's lifetime; removing one on
// disconnect (and everything that implies - detecting the disconnect,
// the 20s auto-resign countdown) is Task D4's job, not this one.
template <typename Handle>
class ConnectionRegistry {
public:
    explicit ConnectionRegistry(size_t maxConnections) : maxConnections_(maxConnections) {}

    bool tryAdd(const Handle& handle) {
        if (connections_.size() >= maxConnections_) return false;
        connections_.push_back(handle);
        return true;
    }

    const std::vector<Handle>& connections() const { return connections_; }
    size_t activeCount() const { return connections_.size(); }

private:
    size_t maxConnections_;
    std::vector<Handle> connections_;
};
#endif
