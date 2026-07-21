#ifndef PERSISTENCE_DATABASE_H
#define PERSISTENCE_DATABASE_H
#include <cstdint>
#include <string>

struct sqlite3;
struct sqlite3_stmt;

// Task C1: thin wrapper over the SQLite C API (third_party/sqlite, vendored
// - see third_party/README.md). Knows nothing about users/games/ratings -
// that's UserRepository's job (Task C2), built on top of this. Dual-
// compiled (Makefile + server/CMakeLists.txt) so it gets doctest coverage,
// same reasoning as GameStateSerializer (A4).
//
// Deliberately two ways to run SQL, not one:
// - exec(): parameterless statements only (CREATE TABLE, PRAGMA, or a
//   literal-only INSERT in a test). Never build a statement by
//   concatenating untrusted values into the string passed here - that's
//   exactly the SQL-injection hole prepare()/bind() exists to close.
// - prepare()/Statement::bind(): anything with a value that came from
//   outside this process (a username, a password hash) MUST go through
//   bound parameters, not string concatenation.
class Statement {
public:
    ~Statement();
    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;
    Statement(Statement&& other) noexcept;
    Statement& operator=(Statement&& other) noexcept;

    // 1-based parameter indices, matching SQLite's own convention (the
    // first "?" in the SQL is index 1, not 0).
    void bind(int index, const std::string& value);
    void bind(int index, int64_t value);
    void bindNull(int index);

    // Advances to the next row. Returns true if a row is available (its
    // columns can be read via column*() below), false once the statement
    // is done (no more rows - this is also the normal return for a bound
    // INSERT/UPDATE, which never has rows in the first place).
    bool step();

    // 0-based column indices, matching SQLite's own convention. Only
    // meaningful after a step() that returned true.
    std::string columnText(int index) const;
    int64_t columnInt64(int index) const;
    bool isNull(int index) const;

private:
    friend class Database;
    explicit Statement(sqlite3_stmt* stmt);
    sqlite3_stmt* stmt_;
};

class Database {
public:
    // path == ":memory:" for an in-memory database (doctest's own
    // convention, see UserRepository's tests, Task C2) - no disk I/O
    // needed for tests, same trick the existing text_io tests already use
    // via istringstream instead of real files.
    explicit Database(const std::string& path);
    ~Database();
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;
    Database(Database&& other) noexcept;
    Database& operator=(Database&& other) noexcept;

    // Throws std::runtime_error (with sqlite3_errmsg's text) on failure.
    void exec(const std::string& sql);
    Statement prepare(const std::string& sql);

private:
    sqlite3* db_;
};
#endif
