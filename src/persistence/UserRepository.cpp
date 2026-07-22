#include "UserRepository.hpp"
#include <sha256.h>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <random>

namespace {

std::string bytesToHex(const unsigned char* data, size_t len) {
    static const char* hexDigits = "0123456789abcdef";
    std::string out;
    out.reserve(len * 2);
    for (size_t i = 0; i < len; ++i) {
        out.push_back(hexDigits[(data[i] >> 4) & 0xF]);
        out.push_back(hexDigits[data[i] & 0xF]);
    }
    return out;
}

// 16 random bytes (128 bits), hex-encoded to a 32-character string for
// storage in password_salt TEXT.
std::string generateSalt() {
    // thread_local: each thread gets its own generator, so concurrent
    // registrations across sessions (once SessionManager, Task D1, exists)
    // never share - or race on - the same std::mt19937_64 state.
    static thread_local std::mt19937_64 rng(std::random_device{}());
    unsigned char bytes[16];
    for (size_t i = 0; i < sizeof(bytes); i += sizeof(uint64_t)) {
        uint64_t r = rng();
        size_t n = std::min(sizeof(uint64_t), sizeof(bytes) - i);
        std::memcpy(bytes + i, &r, n);
    }
    return bytesToHex(bytes, sizeof(bytes));
}

// SHA-256(salt + password), hex-encoded to a 64-character digest.
std::string hashPassword(const std::string& salt, const std::string& password) {
    std::string combined = salt + password;
    SHA256_CTX ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, reinterpret_cast<const unsigned char*>(combined.data()), combined.size());
    unsigned char digest[SHA256_BLOCK_SIZE];
    sha256_final(&ctx, digest);
    return bytesToHex(digest, SHA256_BLOCK_SIZE);
}

int64_t nowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

} // namespace

UserRepository::UserRepository(Database& db) : db_(db) {}

void UserRepository::ensureSchema() {
    db_.exec(
        "CREATE TABLE IF NOT EXISTS users ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  username TEXT NOT NULL UNIQUE,"
        "  password_hash TEXT NOT NULL,"
        "  password_salt TEXT NOT NULL,"
        "  rating INTEGER NOT NULL DEFAULT 1200,"
        "  created_at INTEGER NOT NULL"
        ")"
    );
}

User UserRepository::createUser(const std::string& username, const std::string& password) {
    std::string salt = generateSalt();
    std::string hash = hashPassword(salt, password);

    Statement insert = db_.prepare(
        "INSERT INTO users (username, password_hash, password_salt, created_at) VALUES (?, ?, ?, ?)");
    insert.bind(1, username);
    insert.bind(2, hash);
    insert.bind(3, salt);
    insert.bind(4, nowMs());
    insert.step(); // throws std::runtime_error if username is already taken (UNIQUE)

    // Re-read the row we just inserted rather than tracking last-insert-
    // rowid ourselves - reuses findByUsername()'s existing column mapping
    // instead of duplicating it, and username is UNIQUE so this can't
    // possibly find someone else's row.
    return *findByUsername(username);
}

std::optional<User> UserRepository::findByUsername(const std::string& username) {
    Statement query = db_.prepare(
        "SELECT id, username, rating, created_at FROM users WHERE username = ?");
    query.bind(1, username);
    if (!query.step()) return std::nullopt;

    User user;
    user.id = query.columnInt64(0);
    user.username = query.columnText(1);
    user.rating = static_cast<int>(query.columnInt64(2));
    user.createdAtMs = query.columnInt64(3);
    return user;
}

bool UserRepository::verifyPassword(const std::string& username, const std::string& password) {
    Statement query = db_.prepare("SELECT password_hash, password_salt FROM users WHERE username = ?");
    query.bind(1, username);
    if (!query.step()) return false; // no such user - see header comment on why this isn't distinguished

    std::string storedHash = query.columnText(0);
    std::string storedSalt = query.columnText(1);
    return hashPassword(storedSalt, password) == storedHash;
}

bool UserRepository::updateRating(const std::string& username, int newRating) {
    if (!findByUsername(username)) return false;

    Statement update = db_.prepare("UPDATE users SET rating = ? WHERE username = ?");
    update.bind(1, static_cast<int64_t>(newRating));
    update.bind(2, username);
    update.step();
    return true;
}
