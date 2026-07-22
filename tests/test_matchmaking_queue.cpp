#include "doctest.h"
#include "MatchmakingQueue.hpp"

// Task D2: MatchmakingQueue is the pure, socket-independent "who plays
// whom" pairing decision - factored out so it's testable without real
// sockets/sessions, same reasoning as ConnectionRegistry (A5)/
// SessionManager (D1). Templated on the seeker-id type so tests can use
// plain ints. Not wired into WebSocketServer yet (Task D3, alongside the
// 60s timeout).

TEST_CASE("two seekers within ELO range are matched") {
    MatchmakingQueue<int> queue;
    queue.enqueue(1, 1200);
    queue.enqueue(2, 1250); // diff = 50

    auto match = queue.tryMatch();
    REQUIRE(match.has_value());
    CHECK(match->first == 1);
    CHECK(match->second == 2);
    CHECK(queue.size() == 0);
}

TEST_CASE("two seekers outside ELO range are not matched") {
    MatchmakingQueue<int> queue;
    queue.enqueue(1, 1200);
    queue.enqueue(2, 1350); // diff = 150

    CHECK(queue.tryMatch().has_value() == false);
    CHECK(queue.size() == 2); // queue left unchanged
}

TEST_CASE("exactly 100 rating difference is matched - the range is inclusive") {
    MatchmakingQueue<int> queue;
    queue.enqueue(1, 1200);
    queue.enqueue(2, 1300); // diff = exactly 100

    CHECK(queue.tryMatch().has_value() == true);
}

TEST_CASE("101 rating difference is not matched - just past the boundary") {
    MatchmakingQueue<int> queue;
    queue.enqueue(1, 1200);
    queue.enqueue(2, 1301); // diff = 101

    CHECK(queue.tryMatch().has_value() == false);
}

TEST_CASE("rating difference is symmetric regardless of which seeker enqueued first") {
    MatchmakingQueue<int> queue;
    queue.enqueue(1, 1300); // higher rating enqueued first this time
    queue.enqueue(2, 1250);

    CHECK(queue.tryMatch().has_value() == true);
}

TEST_CASE("a valid pair is found even when a non-adjacent seeker sits between them") {
    // Task D2's own stated requirement: "symmetric check, not just
    // adjacent-in-queue-order". A(1200) and C(1250) are in range even
    // though B(1600), enqueued between them, is not in range with either.
    MatchmakingQueue<int> queue;
    queue.enqueue(1, 1200); // A
    queue.enqueue(2, 1600); // B
    queue.enqueue(3, 1250); // C

    auto match = queue.tryMatch();
    REQUIRE(match.has_value());
    CHECK(match->first == 1);
    CHECK(match->second == 3);
    CHECK(queue.size() == 1); // B (id 2) remains queued
    CHECK(queue.contains(2) == true);
}

TEST_CASE("tryMatch on an empty queue returns nullopt") {
    MatchmakingQueue<int> queue;
    CHECK(queue.tryMatch().has_value() == false);
}

TEST_CASE("tryMatch with only one seeker queued returns nullopt") {
    MatchmakingQueue<int> queue;
    queue.enqueue(1, 1200);
    CHECK(queue.tryMatch().has_value() == false);
    CHECK(queue.size() == 1);
}

TEST_CASE("remove takes a seeker out of the queue without matching them") {
    MatchmakingQueue<int> queue;
    queue.enqueue(1, 1200);
    queue.enqueue(2, 1250);

    CHECK(queue.remove(1) == true);
    CHECK(queue.size() == 1);
    CHECK(queue.contains(1) == false);
    CHECK(queue.contains(2) == true);
    // Only one seeker left - can't match anyone even though it would have
    // been in range with the one just removed.
    CHECK(queue.tryMatch().has_value() == false);
}

TEST_CASE("remove on a seeker not in the queue returns false") {
    MatchmakingQueue<int> queue;
    queue.enqueue(1, 1200);
    CHECK(queue.remove(999) == false);
    CHECK(queue.size() == 1);
}

TEST_CASE("contains reports queued and non-queued seekers correctly") {
    MatchmakingQueue<int> queue;
    CHECK(queue.contains(1) == false);
    queue.enqueue(1, 1200);
    CHECK(queue.contains(1) == true);
}
