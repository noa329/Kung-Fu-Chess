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

} // namespace

namespace GameStateSerializer {

std::string serialize(const GameSnapshot& snapshot) {
    json j;
    j["board"] = snapshot.boardTokens;
    j["cellStates"] = snapshot.cellStates;
    j["whiteScore"] = snapshot.whiteScore;
    j["blackScore"] = snapshot.blackScore;
    j["whiteMoves"] = moveRecordsToJson(snapshot.whiteMoves);
    j["blackMoves"] = moveRecordsToJson(snapshot.blackMoves);
    j["gameOver"] = snapshot.gameOver;
    j["result"] = snapshot.result;
    return j.dump();
}

} // namespace GameStateSerializer
