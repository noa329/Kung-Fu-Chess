#include "doctest.h"
#include "DisconnectTimeout.hpp"

// Task D4: DisconnectTimeout::shouldAutoResign is the pure "has this
// disconnected seat waited too long" decision - decoupled from real
// timers, so it's testable without waiting 20 real seconds. The real
// steady_timer glue (WebSocketServer, manually verified separately) is
// what measures actual elapsed time and current seat connection state to
// feed in here.

TEST_CASE("a seat still disconnected past the timeout should auto-resign") {
    CHECK(DisconnectTimeout::shouldAutoResign(20000, true) == true);
    CHECK(DisconnectTimeout::shouldAutoResign(25000, true) == true);
}

TEST_CASE("a seat still disconnected before the timeout should not auto-resign yet") {
    CHECK(DisconnectTimeout::shouldAutoResign(19999, true) == false);
    CHECK(DisconnectTimeout::shouldAutoResign(0, true) == false);
}

TEST_CASE("a seat that reconnected should not auto-resign even past the deadline") {
    // Reconnected in the window between the timer being scheduled and
    // firing - resigning them anyway would be wrong.
    CHECK(DisconnectTimeout::shouldAutoResign(25000, false) == false);
}
