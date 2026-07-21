#include "doctest.h"
#include "Database.hpp"
#include <stdexcept>

// Task C1: persistence/Database, a thin wrapper over the SQLite C API
// (third_party/sqlite, vendored - see third_party/README.md). Knows
// nothing about users/games/ratings - that's UserRepository's job
// (Task C2). Every test uses ":memory:" - no disk I/O, same trick the
// existing text_io tests already use via istringstream instead of real
// files.

TEST_CASE("create table, insert, and query round-trip") {
    Database db(":memory:");
    db.exec("CREATE TABLE widgets (id INTEGER PRIMARY KEY, name TEXT NOT NULL, count INTEGER NOT NULL)");

    Statement insert = db.prepare("INSERT INTO widgets (name, count) VALUES (?, ?)");
    insert.bind(1, std::string("bolt"));
    insert.bind(2, static_cast<int64_t>(42));
    CHECK(insert.step() == false); // INSERT has no rows - false means "done", not an error

    Statement query = db.prepare("SELECT name, count FROM widgets WHERE name = ?");
    query.bind(1, std::string("bolt"));
    REQUIRE(query.step() == true);
    CHECK(query.columnText(0) == "bolt");
    CHECK(query.columnInt64(1) == 42);
    CHECK(query.step() == false); // only one matching row
}

TEST_CASE("a query with no matching rows returns no rows") {
    Database db(":memory:");
    db.exec("CREATE TABLE widgets (id INTEGER PRIMARY KEY, name TEXT NOT NULL)");

    Statement query = db.prepare("SELECT name FROM widgets WHERE name = ?");
    query.bind(1, std::string("nonexistent"));
    CHECK(query.step() == false);
}

TEST_CASE("multiple rows are iterated in insertion order") {
    Database db(":memory:");
    db.exec("CREATE TABLE widgets (id INTEGER PRIMARY KEY, name TEXT NOT NULL)");

    Statement insertA = db.prepare("INSERT INTO widgets (name) VALUES (?)");
    insertA.bind(1, std::string("first"));
    insertA.step();
    Statement insertB = db.prepare("INSERT INTO widgets (name) VALUES (?)");
    insertB.bind(1, std::string("second"));
    insertB.step();

    Statement query = db.prepare("SELECT name FROM widgets ORDER BY id");
    REQUIRE(query.step() == true);
    CHECK(query.columnText(0) == "first");
    REQUIRE(query.step() == true);
    CHECK(query.columnText(0) == "second");
    CHECK(query.step() == false);
}

TEST_CASE("a null column reads as null via isNull") {
    Database db(":memory:");
    db.exec("CREATE TABLE widgets (id INTEGER PRIMARY KEY, note TEXT)");

    Statement insert = db.prepare("INSERT INTO widgets (note) VALUES (?)");
    insert.bindNull(1);
    insert.step();

    Statement query = db.prepare("SELECT note FROM widgets");
    REQUIRE(query.step() == true);
    CHECK(query.isNull(0) == true);
}

TEST_CASE("exec on malformed SQL throws") {
    Database db(":memory:");
    CHECK_THROWS_AS(db.exec("NOT VALID SQL"), std::runtime_error);
}

TEST_CASE("prepare on malformed SQL throws") {
    Database db(":memory:");
    CHECK_THROWS_AS(db.prepare("NOT VALID SQL"), std::runtime_error);
}
