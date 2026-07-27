#include "DisconnectStatusDeserializer.hpp"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace DisconnectStatusDeserializer {

std::optional<DisconnectStatus> deserialize(const std::string& raw) {
    try {
        json j = json::parse(raw);
        if (!j.is_object() || !j.contains("board")) return std::nullopt;

        DisconnectStatus status;
        if (j.contains("whiteDisconnectMs") && !j.at("whiteDisconnectMs").is_null()) {
            status.whiteRemainingMs = j.at("whiteDisconnectMs").get<int>();
        }
        if (j.contains("blackDisconnectMs") && !j.at("blackDisconnectMs").is_null()) {
            status.blackRemainingMs = j.at("blackDisconnectMs").get<int>();
        }
        return status;
    } catch (const json::exception&) {
        return std::nullopt;
    }
}

} // namespace DisconnectStatusDeserializer
