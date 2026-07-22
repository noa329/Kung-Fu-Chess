#include "doctest.h"
#include "SessionManager.hpp"

// Task D1: SessionManager is the pure, socket-independent piece of
// WebSocketServer's "which session does this connection belong to"
// decision - factored out so it's testable without real sockets, same
// reasoning as ConnectionRegistry (A5). Templated on the connection-id
// type so tests can use plain ints instead of websocketpp::connection_hdl.

TEST_CASE("createSession returns sequential ids starting at 0") {
    SessionManager<int> manager(2);
    CHECK(manager.createSession() == 0);
    CHECK(manager.createSession() == 1);
    CHECK(manager.createSession() == 2);
    CHECK(manager.sessionCount() == 3);
}

TEST_CASE("connections are accepted into a session up to its capacity") {
    SessionManager<int> manager(2);
    int session = manager.createSession();
    CHECK(manager.tryAdd(session, 1) == true);
    CHECK(manager.tryAdd(session, 2) == true);
    CHECK(manager.occupancy(session) == 2);
}

TEST_CASE("a connection beyond a session's capacity is rejected") {
    SessionManager<int> manager(2);
    int session = manager.createSession();
    manager.tryAdd(session, 1);
    manager.tryAdd(session, 2);
    CHECK(manager.tryAdd(session, 3) == false);
    CHECK(manager.occupancy(session) == 2); // rejection doesn't add a phantom slot
}

TEST_CASE("tryAdd never auto-creates a session - an unknown session id is rejected") {
    SessionManager<int> manager(2);
    CHECK(manager.tryAdd(0, 1) == false); // no createSession() call yet
    CHECK(manager.sessionCount() == 0);
}

TEST_CASE("tryAdd rejects a negative session id") {
    SessionManager<int> manager(2);
    manager.createSession();
    CHECK(manager.tryAdd(-1, 1) == false);
}

TEST_CASE("sessionFor reports which session a connection belongs to") {
    SessionManager<int> manager(2);
    int session = manager.createSession();
    manager.tryAdd(session, 42);
    CHECK(manager.sessionFor(42) == std::optional<int>(session));
}

TEST_CASE("sessionFor returns nullopt for a connection that was never added") {
    SessionManager<int> manager(2);
    manager.createSession();
    CHECK(manager.sessionFor(999).has_value() == false);
}

TEST_CASE("connectionsIn returns a session's connections in insertion order") {
    SessionManager<int> manager(2);
    int session = manager.createSession();
    manager.tryAdd(session, 10);
    manager.tryAdd(session, 20);
    CHECK(manager.connectionsIn(session) == std::vector<int>{10, 20});
}

TEST_CASE("independent sessions don't share capacity or connection lists") {
    SessionManager<int> manager(2);
    int sessionA = manager.createSession();
    int sessionB = manager.createSession();

    manager.tryAdd(sessionA, 1);
    manager.tryAdd(sessionA, 2);
    CHECK(manager.tryAdd(sessionB, 3) == true); // sessionA being full doesn't affect sessionB
    CHECK(manager.occupancy(sessionB) == 1);
    CHECK(manager.connectionsIn(sessionA) == std::vector<int>{1, 2});
    CHECK(manager.connectionsIn(sessionB) == std::vector<int>{3});
    CHECK(manager.sessionFor(3) == std::optional<int>(sessionB));
}
