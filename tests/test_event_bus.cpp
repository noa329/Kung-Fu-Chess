#include "doctest.h"
#include "EventBus.hpp"

// event_bus layer: Signal<T>/EventBus pub-sub plumbing only - no game logic,
// no knowledge of GameEngine or any subscriber (SoundManager, renderer, etc).

TEST_CASE("publish with no subscribers is a safe no-op") {
    Signal<SoundEvent> signal;
    signal.publish({"move"}); // must not throw/crash
}

TEST_CASE("a subscribed handler receives published events") {
    Signal<SoundEvent> signal;
    std::vector<std::string> received;
    signal.subscribe([&received](const SoundEvent& e) { received.push_back(e.name); });

    signal.publish({"move"});
    signal.publish({"capture"});

    CHECK(received == std::vector<std::string>{"move", "capture"});
}

TEST_CASE("multiple subscribers all receive the same event") {
    Signal<SoundEvent> signal;
    int callCountA = 0;
    int callCountB = 0;
    signal.subscribe([&callCountA](const SoundEvent&) { ++callCountA; });
    signal.subscribe([&callCountB](const SoundEvent&) { ++callCountB; });

    signal.publish({"jump"});

    CHECK(callCountA == 1);
    CHECK(callCountB == 1);
}

TEST_CASE("unsubscribe stops a handler from receiving further events") {
    Signal<SoundEvent> signal;
    int callCount = 0;
    size_t token = signal.subscribe([&callCount](const SoundEvent&) { ++callCount; });

    signal.publish({"move"});
    signal.unsubscribe(token);
    signal.publish({"move"});

    CHECK(callCount == 1);
}

TEST_CASE("unsubscribing one token leaves other subscribers intact") {
    Signal<SoundEvent> signal;
    int callCountA = 0;
    int callCountB = 0;
    size_t tokenA = signal.subscribe([&callCountA](const SoundEvent&) { ++callCountA; });
    signal.subscribe([&callCountB](const SoundEvent&) { ++callCountB; });

    signal.unsubscribe(tokenA);
    signal.publish({"jump"});

    CHECK(callCountA == 0);
    CHECK(callCountB == 1);
}

TEST_CASE("unsubscribing an unknown token is a safe no-op") {
    Signal<SoundEvent> signal;
    signal.unsubscribe(12345); // never subscribed - must not throw/crash
}

TEST_CASE("EventBus exposes one independently-triggerable Signal per event category") {
    EventBus bus;
    std::vector<std::string> soundNames;
    std::vector<int> scoreDeltas;

    bus.onSound.subscribe([&soundNames](const SoundEvent& e) { soundNames.push_back(e.name); });
    bus.onScoreUpdated.subscribe([&scoreDeltas](const ScoreUpdatedEvent& e) { scoreDeltas.push_back(e.delta); });

    bus.onSound.publish({"capture"});
    bus.onScoreUpdated.publish({'w', 9, 9});

    CHECK(soundNames == std::vector<std::string>{"capture"});
    CHECK(scoreDeltas == std::vector<int>{9});
}
