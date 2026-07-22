#ifndef PERSISTENCE_USER_REPOSITORY_H
#define PERSISTENCE_USER_REPOSITORY_H
#include "Database.hpp"
#include <cstdint>
#include <optional>
#include <string>

// Task C2: users table CRUD + rating read/update, built on Database (C1).
// Schema (see docs/tasks/server-phase-plan.md):
//   CREATE TABLE users (
//       id            INTEGER PRIMARY KEY AUTOINCREMENT,
//       username      TEXT NOT NULL UNIQUE,
//       password_hash TEXT NOT NULL,
//       password_salt TEXT NOT NULL,
//       rating        INTEGER NOT NULL DEFAULT 1200,
//       created_at    INTEGER NOT NULL  -- unix ms
//   );
//
// Passwords are never stored in plaintext, and there is no plaintext to
// read back: createUser() generates a random salt and stores only
// SHA-256(salt + password) (third_party/sha256, Brad Conte's public-domain
// implementation - see third_party/README.md); verifyPassword() re-hashes
// the candidate password with the stored salt and compares digests. Both
// are private helpers in UserRepository.cpp, not a separate class - this
// is the only place in the codebase that needs them (Task C3's
// AuthService calls verifyPassword(), it doesn't hash anything itself).
struct User {
    int64_t id;
    std::string username;
    int rating;
    int64_t createdAtMs;
};

class UserRepository {
public:
    explicit UserRepository(Database& db);

    // CREATE TABLE IF NOT EXISTS - idempotent, safe to call every startup.
    void ensureSchema();

    // Throws std::runtime_error if the username is already taken (the
    // table's own UNIQUE constraint - Database::prepare()/Statement::step()
    // surface that as an exception already, so there's no separate
    // pre-check to write here). Task C4 (auto-register) is the caller's
    // job: check findByUsername() first, only call createUser() for a
    // never-seen-before username.
    User createUser(const std::string& username, const std::string& password);

    std::optional<User> findByUsername(const std::string& username);

    // True only if a user with that username exists AND password matches
    // (re-hashed with that user's stored salt) it. False for a
    // non-existent username too - same "no such user" outcome as a wrong
    // password, deliberately: telling a login attempt "wrong password"
    // vs. "no such user" would leak which usernames are registered.
    bool verifyPassword(const std::string& username, const std::string& password);

    // False (no-op) if no user with that username exists.
    bool updateRating(const std::string& username, int newRating);

private:
    Database& db_;
};
#endif
