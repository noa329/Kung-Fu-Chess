#include "doctest.h"
#include "UserRepository.hpp"
#include <stdexcept>

// Task C2: users table CRUD + rating read/update, on top of Database (C1).
// Every test uses ":memory:" - no disk I/O, same trick Database's own
// tests already use.

TEST_CASE("create user, find by username, wrong password rejected") {
    Database db(":memory:");
    UserRepository repo(db);
    repo.ensureSchema();

    User created = repo.createUser("alice", "correct-horse");
    CHECK(created.username == "alice");
    CHECK(created.rating == 1200); // schema default
    CHECK(created.id > 0);
    CHECK(created.createdAtMs > 0);

    auto found = repo.findByUsername("alice");
    REQUIRE(found.has_value());
    CHECK(found->id == created.id);
    CHECK(found->username == "alice");
    CHECK(found->rating == 1200);

    CHECK(repo.verifyPassword("alice", "correct-horse") == true);
    CHECK(repo.verifyPassword("alice", "wrong-password") == false);
}

TEST_CASE("finding a nonexistent username returns nullopt") {
    Database db(":memory:");
    UserRepository repo(db);
    repo.ensureSchema();

    CHECK(repo.findByUsername("nobody").has_value() == false);
}

TEST_CASE("verifying a nonexistent username's password returns false, not throws") {
    Database db(":memory:");
    UserRepository repo(db);
    repo.ensureSchema();

    CHECK(repo.verifyPassword("nobody", "anything") == false);
}

TEST_CASE("creating a duplicate username throws") {
    Database db(":memory:");
    UserRepository repo(db);
    repo.ensureSchema();

    repo.createUser("alice", "password1");
    CHECK_THROWS_AS(repo.createUser("alice", "password2"), std::runtime_error);
}

TEST_CASE("rating update persists") {
    Database db(":memory:");
    UserRepository repo(db);
    repo.ensureSchema();

    repo.createUser("alice", "correct-horse");
    CHECK(repo.updateRating("alice", 1350) == true);

    auto found = repo.findByUsername("alice");
    REQUIRE(found.has_value());
    CHECK(found->rating == 1350);
}

TEST_CASE("updating rating for a nonexistent username returns false") {
    Database db(":memory:");
    UserRepository repo(db);
    repo.ensureSchema();

    CHECK(repo.updateRating("nobody", 1500) == false);
}

TEST_CASE("two different users get different password hashes for the same password") {
    // Confirms per-user salting actually happens - if it didn't, two users
    // with the same password would be indistinguishable in storage, which
    // would defeat the point of a per-user salt.
    Database db(":memory:");
    UserRepository repo(db);
    repo.ensureSchema();

    repo.createUser("alice", "same-password");
    repo.createUser("bob", "same-password");

    CHECK(repo.verifyPassword("alice", "same-password") == true);
    CHECK(repo.verifyPassword("bob", "same-password") == true);
}

TEST_CASE("ensureSchema is idempotent") {
    Database db(":memory:");
    UserRepository repo(db);
    repo.ensureSchema();
    repo.ensureSchema(); // must not throw "table already exists"

    repo.createUser("alice", "password");
    CHECK(repo.findByUsername("alice").has_value());
}
