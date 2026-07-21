#include "Database.hpp"
#include <sqlite3.h>
#include <stdexcept>
#include <utility>

namespace {
[[noreturn]] void throwSqliteError(sqlite3* db, const std::string& context) {
    throw std::runtime_error(context + ": " + sqlite3_errmsg(db));
}
} // namespace

Statement::Statement(sqlite3_stmt* stmt) : stmt_(stmt) {}

Statement::~Statement() {
    sqlite3_finalize(stmt_); // no-op if stmt_ is null (moved-from or never set)
}

Statement::Statement(Statement&& other) noexcept : stmt_(other.stmt_) {
    other.stmt_ = nullptr;
}

Statement& Statement::operator=(Statement&& other) noexcept {
    if (this != &other) {
        sqlite3_finalize(stmt_);
        stmt_ = other.stmt_;
        other.stmt_ = nullptr;
    }
    return *this;
}

void Statement::bind(int index, const std::string& value) {
    // SQLITE_TRANSIENT tells SQLite to copy the string internally - value
    // may go out of scope right after this call returns.
    sqlite3_bind_text(stmt_, index, value.c_str(), static_cast<int>(value.size()), SQLITE_TRANSIENT);
}

void Statement::bind(int index, int64_t value) {
    sqlite3_bind_int64(stmt_, index, value);
}

void Statement::bindNull(int index) {
    sqlite3_bind_null(stmt_, index);
}

bool Statement::step() {
    int rc = sqlite3_step(stmt_);
    if (rc == SQLITE_ROW) return true;
    if (rc == SQLITE_DONE) return false;
    throw std::runtime_error(std::string("sqlite3_step failed: ") + sqlite3_errstr(rc));
}

std::string Statement::columnText(int index) const {
    const unsigned char* text = sqlite3_column_text(stmt_, index);
    return text ? reinterpret_cast<const char*>(text) : "";
}

int64_t Statement::columnInt64(int index) const {
    return sqlite3_column_int64(stmt_, index);
}

bool Statement::isNull(int index) const {
    return sqlite3_column_type(stmt_, index) == SQLITE_NULL;
}

Database::Database(const std::string& path) : db_(nullptr) {
    if (sqlite3_open(path.c_str(), &db_) != SQLITE_OK) {
        std::string message = db_ ? sqlite3_errmsg(db_) : "unknown error";
        sqlite3_close(db_); // sqlite3_open still allocates a handle on failure
        throw std::runtime_error("failed to open database '" + path + "': " + message);
    }
}

Database::~Database() {
    sqlite3_close(db_); // no-op if db_ is null (moved-from)
}

Database::Database(Database&& other) noexcept : db_(other.db_) {
    other.db_ = nullptr;
}

Database& Database::operator=(Database&& other) noexcept {
    if (this != &other) {
        sqlite3_close(db_);
        db_ = other.db_;
        other.db_ = nullptr;
    }
    return *this;
}

void Database::exec(const std::string& sql) {
    char* errMsg = nullptr;
    int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::string message = errMsg ? errMsg : "unknown error";
        sqlite3_free(errMsg);
        throw std::runtime_error("exec failed: " + message);
    }
}

Statement Database::prepare(const std::string& sql) {
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql.c_str(), static_cast<int>(sql.size()), &stmt, nullptr) != SQLITE_OK) {
        throwSqliteError(db_, "prepare failed");
    }
    return Statement(stmt);
}
