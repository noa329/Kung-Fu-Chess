#include "GameStateDeserializer.hpp"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace {

MoveRecord moveRecordFromJson(const json& j) {
    MoveRecord m;
    m.atMs = j.value("atMs", 0LL);
    std::string colorStr = j.value("color", std::string());
    m.color = colorStr.empty() ? '\0' : colorStr[0];
    m.notation = j.value("notation", std::string());
    return m;
}

std::vector<MoveRecord> moveRecordsFromJson(const json& arr) {
    std::vector<MoveRecord> moves;
    if (!arr.is_array()) return moves;
    for (const auto& item : arr) moves.push_back(moveRecordFromJson(item));
    return moves;
}

// Task I2: inverse of GameStateSerializer.hpp's activeMovesToJson() - reads
// the sparse "activeMoves" list (absent, or an empty array, when nothing is
// mid-move) and fills in exactly the cells it names. snap.moveTargets/
// moveProgress must already be sized+defaulted before this runs. Malformed
// entries (missing keys, an out-of-bounds row/col) are skipped rather than
// thrown - same "network boundary tolerates junk" stance the rest of this
// file already takes, not a guess about what a well-behaved server sends.
void applyActiveMoves(const json& j, GameSnapshot& snap) {
    if (!j.contains("activeMoves") || !j.at("activeMoves").is_array()) return;

    for (const auto& entry : j.at("activeMoves")) {
        if (!entry.is_object() || !entry.contains("from") || !entry.contains("to")) continue;
        const json& from = entry.at("from");
        const json& to = entry.at("to");
        if (!from.is_object() || !to.is_object()) continue;

        int r = from.value("row", -1);
        int c = from.value("col", -1);
        if (r < 0 || c < 0 || static_cast<size_t>(r) >= snap.moveTargets.size() ||
            static_cast<size_t>(c) >= snap.moveTargets[r].size()) {
            continue;
        }

        snap.moveTargets[r][c] = Position{to.value("row", -1), to.value("col", -1)};
        snap.moveProgress[r][c] = entry.value("progress", 0.0);
    }
}

} // namespace

namespace GameStateDeserializer {

std::optional<GameSnapshot> deserialize(const std::string& raw) {
    try {
        json j = json::parse(raw);
        if (!j.is_object() || !j.contains("board")) return std::nullopt;

        GameSnapshot snap;
        snap.boardTokens = j.at("board").get<std::vector<std::vector<std::string>>>();
        snap.cellStates = j.value("cellStates", std::vector<std::vector<std::string>>{});
        snap.whiteScore = j.value("whiteScore", 0);
        snap.blackScore = j.value("blackScore", 0);
        snap.whiteMoves = moveRecordsFromJson(j.value("whiteMoves", json::array()));
        snap.blackMoves = moveRecordsFromJson(j.value("blackMoves", json::array()));
        snap.gameOver = j.value("gameOver", false);
        snap.result = j.value("result", std::string());

        // See this header's own comment: selected/captureFlashes never
        // arrive on the wire at all, defaulted here rather than guessed at.
        snap.selected = Position{-1, -1};
        size_t rows = snap.boardTokens.size();
        size_t cols = rows > 0 ? snap.boardTokens[0].size() : 0;
        snap.moveTargets.assign(rows, std::vector<Position>(cols, Position{-1, -1}));
        snap.moveProgress.assign(rows, std::vector<double>(cols, 0.0));
        snap.captureFlashes.clear();

        // Task I2: overwrite the defaults above for exactly the cells the
        // wire actually reports as mid-move.
        applyActiveMoves(j, snap);

        return snap;
    } catch (const json::exception&) {
        // Covers both a parse failure and a "board" key present but the
        // wrong shape (e.g. not an array of arrays of strings) - this is a
        // network boundary, so malformed input is reported as "not a state
        // broadcast", not an exception the caller has to guard against.
        return std::nullopt;
    }
}

} // namespace GameStateDeserializer
