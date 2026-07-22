#include "doctest.h"
#include "AuthService.hpp"

// Task C3: login/register decision logic, decoupled from the socket layer
// - built directly against a real (":memory:") UserRepository/Database,
// same pattern UserRepository's own tests already use.

TEST_CASE("a never-seen-before username is auto-registered and accepted") {
    Database db(":memory:");
    UserRepository users(db);
    users.ensureSchema();
    AuthService auth(users);

    auto result = auth.authenticate("alice", "correct-horse");
    CHECK(result.ok);
    CHECK(result.isNewAccount == true);
    CHECK(result.user.username == "alice");
    CHECK(result.user.rating == 1200);

    CHECK(users.findByUsername("alice").has_value());
}

TEST_CASE("an existing username with the correct password is accepted, not re-registered") {
    Database db(":memory:");
    UserRepository users(db);
    users.ensureSchema();
    AuthService auth(users);

    auth.authenticate("alice", "correct-horse"); // auto-registers
    auto result = auth.authenticate("alice", "correct-horse");
    CHECK(result.ok);
    CHECK(result.isNewAccount == false);
    CHECK(result.user.username == "alice");
}

TEST_CASE("an existing username with the wrong password is rejected") {
    Database db(":memory:");
    UserRepository users(db);
    users.ensureSchema();
    AuthService auth(users);

    auth.authenticate("alice", "correct-horse"); // auto-registers with this password
    auto result = auth.authenticate("alice", "wrong-password");
    CHECK(result.ok == false);
    CHECK(result.error == "ERROR AUTH_FAILED");
}

TEST_CASE("auto-registration locks in the first password - a later different password fails") {
    Database db(":memory:");
    UserRepository users(db);
    users.ensureSchema();
    AuthService auth(users);

    auto first = auth.authenticate("alice", "first-password");
    REQUIRE(first.ok);
    REQUIRE(first.isNewAccount);

    auto second = auth.authenticate("alice", "different-password");
    CHECK(second.ok == false);
}

TEST_CASE("two different usernames auto-register independently") {
    Database db(":memory:");
    UserRepository users(db);
    users.ensureSchema();
    AuthService auth(users);

    auto alice = auth.authenticate("alice", "alices-password");
    auto bob = auth.authenticate("bob", "bobs-password");
    CHECK(alice.ok);
    CHECK(bob.ok);
    CHECK(alice.user.id != bob.user.id);

    // Cross-checking the other user's password against this username must
    // still fail - confirms accounts aren't accidentally interchangeable.
    auto crossed = auth.authenticate("alice", "bobs-password");
    CHECK(crossed.ok == false);
}
