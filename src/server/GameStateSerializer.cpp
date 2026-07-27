#include "GameStateSerializer.hpp"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace {

// nlohmann::json treats char as a numeric type by default, so MoveRecord's
// color field needs an explicit std::string(1, ...) conversion here or it
// would serialize as an integer (e.g. 119 for 'w') instead of "w".
json moveRecordToJson(const MoveRecord& m) {
    json j;
    j["atMs"] = m.atMs;
    j["color"] = std::string(1, m.color);
    j["notation"] = m.notation;
    return j;
}

json moveRecordsToJson(const std::vector<MoveRecord>& moves) {
    json arr = json::array();
    for (const auto& m : moves) arr.push_back(moveRecordToJson(m));
    return arr;
}

// Task I1: sparse - only cells where cellStates[r][c] == "move" contribute
// an entry, matching exactly the condition GameEngine::snapshot() already
// uses to populate moveTargets/moveProgress in the first place (see
// docs/tasks/wire-protocol-move-progress-plan.md, Decision 1). Bounds are
// checked against moveTargets/moveProgress independently of cellStates
// since a hand-built GameSnapshot (e.g. in tests) may leave those vectors
// empty or a different size than cellStates.
json activeMovesToJson(const GameSnapshot& snapshot) {
    json arr = json::array();
    for (size_t r = 0; r < snapshot.cellStates.size(); ++r) {
        if (r >= snapshot.moveTargets.size() || r >= snapshot.moveProgress.size()) continue;
        for (size_t c = 0; c < snapshot.cellStates[r].size(); ++c) {
            if (c >= snapshot.moveTargets[r].size() || c >= snapshot.moveProgress[r].size()) continue;
            if (snapshot.cellStates[r][c] != "move") continue;

            const Position& to = snapshot.moveTargets[r][c];
            json entry;
            entry["from"] = json{{"row", static_cast<int>(r)}, {"col", static_cast<int>(c)}};
            entry["to"] = json{{"row", to.row}, {"col", to.col}};
            entry["progress"] = snapshot.moveProgress[r][c];
            arr.push_back(entry);
        }
    }
    return arr;
}

} // namespace

namespace GameStateSerializer {

std::string serialize(const GameSnapshot& snapshot, const DisconnectStatus& disconnect) {
    json j;
    j["board"] = snapshot.boardTokens;
    j["cellStates"] = snapshot.cellStates;
    j["whiteScore"] = snapshot.whiteScore;
    j["blackScore"] = snapshot.blackScore;
    j["whiteMoves"] = moveRecordsToJson(snapshot.whiteMoves);
    j["blackMoves"] = moveRecordsToJson(snapshot.blackMoves);
    j["gameOver"] = snapshot.gameOver;
    j["result"] = snapshot.result;
    j["whiteDisconnectMs"] = disconnect.whiteRemainingMs ? json(*disconnect.whiteRemainingMs) : json(nullptr);
    j["blackDisconnectMs"] = disconnect.blackRemainingMs ? json(*disconnect.blackRemainingMs) : json(nullptr);
    j["activeMoves"] = activeMovesToJson(snapshot);
    return j.dump();
}

} // namespace GameStateSerializer
