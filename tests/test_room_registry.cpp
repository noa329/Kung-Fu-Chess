#include "doctest.h"
#include "RoomRegistry.hpp"
#include <deque>
#include <memory>
#include <queue>
#include <vector>

// Task E1: RoomRegistry is the pure room-ID <-> session-ID discovery
// piece - decoupled from GameSession/sockets entirely, same "pure
// decision, composition root does the wiring" split MatchmakingQueue (D2)
// and SessionManager (D1) already established. idGenerator is injected so
// these tests don't depend on true randomness - see RoomRegistry.hpp's
// constructor comment.

namespace {

// Returns a fixed sequence of ids, one per call - lets a test control
// exactly what RoomRegistry::createRoom() sees, including forcing a
// collision to exercise the retry loop.
std::function<std::string()> fixedSequence(std::vector<std::string> ids) {
    auto queue = std::make_shared<std::queue<std::string>>(
        std::deque<std::string>(ids.begin(), ids.end()));
    return [queue]() {
        std::string next = queue->front();
        queue->pop();
        return next;
    };
}

} // namespace

TEST_CASE("createRoom returns the generator's id and it's the one used") {
    RoomRegistry registry(fixedSequence({"AAAAAA"}));
    std::string id = registry.createRoom(0);
    CHECK(id == "AAAAAA");
}

TEST_CASE("sessionForRoom looks up the session a room id was created for") {
    RoomRegistry registry(fixedSequence({"AAAAAA"}));
    std::string id = registry.createRoom(42);
    CHECK(registry.sessionForRoom(id) == std::optional<int>(42));
}

TEST_CASE("sessionForRoom returns nullopt for an id that was never created") {
    RoomRegistry registry(fixedSequence({"AAAAAA"}));
    CHECK(registry.sessionForRoom("ZZZZZZ").has_value() == false);
}

TEST_CASE("independent rooms are tracked independently") {
    RoomRegistry registry(fixedSequence({"AAAAAA", "BBBBBB"}));
    std::string idA = registry.createRoom(1);
    std::string idB = registry.createRoom(2);
    CHECK(idA != idB);
    CHECK(registry.sessionForRoom(idA) == std::optional<int>(1));
    CHECK(registry.sessionForRoom(idB) == std::optional<int>(2));
}

TEST_CASE("a generator collision is retried until a unique id is produced") {
    // First call for room 2 collides with room 1's already-registered
    // "AAAAAA" - createRoom() must not accept it, and must keep calling
    // the generator until "CCCCCC" (the first actually-unique one).
    RoomRegistry registry(fixedSequence({"AAAAAA", "AAAAAA", "CCCCCC"}));
    std::string idA = registry.createRoom(1);
    std::string idC = registry.createRoom(2);
    CHECK(idA == "AAAAAA");
    CHECK(idC == "CCCCCC");
    CHECK(registry.sessionForRoom("AAAAAA") == std::optional<int>(1));
    CHECK(registry.sessionForRoom("CCCCCC") == std::optional<int>(2));
}

// The real (non-injected) generator - format/uniqueness properties only,
// since it's genuinely random.

TEST_CASE("the real RoomIdGenerator produces 6-character codes") {
    std::string id = RoomIdGenerator::generate();
    CHECK(id.size() == 6);
}

TEST_CASE("the real RoomIdGenerator excludes visually ambiguous characters") {
    std::string id = RoomIdGenerator::generate();
    for (char c : id) {
        CHECK(c != '0');
        CHECK(c != '1');
        CHECK(c != 'I');
        CHECK(c != 'O');
        CHECK(c != 'i');
        CHECK(c != 'o');
        CHECK(c != 'l');
    }
}

TEST_CASE("the real RoomRegistry (default generator) produces distinct ids across many creates") {
    RoomRegistry registry;
    std::vector<std::string> ids;
    for (int i = 0; i < 50; ++i) {
        ids.push_back(registry.createRoom(i));
    }
    for (size_t i = 0; i < ids.size(); ++i) {
        for (size_t j = i + 1; j < ids.size(); ++j) {
            CHECK(ids[i] != ids[j]);
        }
    }
}
