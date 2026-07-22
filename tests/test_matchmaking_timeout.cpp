#include "doctest.h"
#include "MatchmakingTimeout.hpp"

// Task D3: MatchmakingTimeout::shouldTimeOut is the pure "has this seeker
// waited too long" decision - decoupled from real timers, so it's
// testable without waiting 60 real seconds. The real steady_timer glue
// (WebSocketServer, manually verified separately) is what measures actual
// elapsed time and current queue membership to feed in here.

TEST_CASE("a seeker still queued past the timeout should time out") {
    CHECK(MatchmakingTimeout::shouldTimeOut(60000, true) == true);
    CHECK(MatchmakingTimeout::shouldTimeOut(70000, true) == true);
}

TEST_CASE("a seeker still queued before the timeout should not time out yet") {
    CHECK(MatchmakingTimeout::shouldTimeOut(59999, true) == false);
    CHECK(MatchmakingTimeout::shouldTimeOut(0, true) == false);
}

TEST_CASE("a seeker no longer queued should not time out even past the deadline") {
    // Matched (or otherwise removed) in the window between the timer
    // being scheduled and firing - timing them out anyway would be wrong.
    CHECK(MatchmakingTimeout::shouldTimeOut(70000, false) == false);
}
