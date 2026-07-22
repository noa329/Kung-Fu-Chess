#ifndef SERVER_CONNECTION_REGISTRY_H
#define SERVER_CONNECTION_REGISTRY_H
#include <algorithm>
#include <cstddef>
#include <functional>
#include <vector>

// Pure, socket-independent capacity tracking - the part of
// WebSocketServer's "accept or reject a new connection" decision that's
// actually testable without real sockets. Templated on the handle type
// so tests can use plain ints; WebSocketServer instantiates it with
// websocketpp::connection_hdl.
//
// remove() (Task D4): frees the slot a disconnected connection held so a
// reconnecting player can tryAdd() back into the same session without
// bumping into the capacity cap. Templated on Compare too (defaulted to
// std::less<Handle>, same default SessionManager uses) - websocketpp::
// connection_hdl is a std::weak_ptr<void> under the hood and has no
// operator==, only the owner_less-based ordering SessionManager already
// carries around for its own map, so remove() has to test equivalence via
// Compare (!comp(a,b) && !comp(b,a)) instead of std::find's operator==.
template <typename Handle, typename Compare = std::less<Handle>>
class ConnectionRegistry {
public:
    explicit ConnectionRegistry(size_t maxConnections) : maxConnections_(maxConnections) {}

    bool tryAdd(const Handle& handle) {
        if (connections_.size() >= maxConnections_) return false;
        connections_.push_back(handle);
        return true;
    }

    // False if handle wasn't present - same "caller can't misuse this"
    // shape as tryAdd()'s bool return.
    bool remove(const Handle& handle) {
        Compare comp;
        auto it = std::find_if(connections_.begin(), connections_.end(),
            [&](const Handle& h) { return !comp(h, handle) && !comp(handle, h); });
        if (it == connections_.end()) return false;
        connections_.erase(it);
        return true;
    }

    const std::vector<Handle>& connections() const { return connections_; }
    size_t activeCount() const { return connections_.size(); }

private:
    size_t maxConnections_;
    std::vector<Handle> connections_;
};
#endif
