#include "RoomRegistry.hpp"
#include <random>

namespace RoomIdGenerator {

std::string generate() {
    // 8 digits (2-9, excludes 0/1) + 24 letters (A-Z, excludes I/O) = 32
    // chars - see RoomRegistry.hpp's comment for why these are excluded.
    static const std::string charset = "23456789ABCDEFGHJKLMNPQRSTUVWXYZ";
    static const int kIdLength = 6;

    // Function-local statics: seeded once per process (not once per call),
    // same std::random_device-seeds-a-real-generator idiom as everywhere
    // else in this codebase that needs real randomness (none yet, but this
    // is the standard library's own recommended pattern).
    static std::mt19937 rng(std::random_device{}());
    static std::uniform_int_distribution<size_t> dist(0, charset.size() - 1);

    std::string id;
    id.reserve(kIdLength);
    for (int i = 0; i < kIdLength; ++i) {
        id += charset[dist(rng)];
    }
    return id;
}

} // namespace RoomIdGenerator
