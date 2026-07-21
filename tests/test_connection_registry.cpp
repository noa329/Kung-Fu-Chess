#include "doctest.h"
#include "ConnectionRegistry.hpp"

// server layer: ConnectionRegistry is the pure, socket-independent piece
// of WebSocketServer's capacity logic (Task A5) - "should a new
// connection be accepted or rejected" - factored out so it's testable
// without real sockets. Templated on the handle type so tests can use
// plain ints instead of websocketpp::connection_hdl.

TEST_CASE("connections are accepted up to the configured maximum") {
    ConnectionRegistry<int> registry(2);
    CHECK(registry.tryAdd(1) == true);
    CHECK(registry.tryAdd(2) == true);
    CHECK(registry.activeCount() == 2);
}

TEST_CASE("a connection beyond the configured maximum is rejected") {
    ConnectionRegistry<int> registry(2);
    registry.tryAdd(1);
    registry.tryAdd(2);
    CHECK(registry.tryAdd(3) == false);
    CHECK(registry.activeCount() == 2); // rejection doesn't add a phantom slot
}

TEST_CASE("accepted connections are retrievable in insertion order") {
    ConnectionRegistry<int> registry(2);
    registry.tryAdd(10);
    registry.tryAdd(20);
    CHECK(registry.connections() == std::vector<int>{10, 20});
}

TEST_CASE("a zero-capacity registry rejects everything") {
    ConnectionRegistry<int> registry(0);
    CHECK(registry.tryAdd(1) == false);
    CHECK(registry.activeCount() == 0);
}
