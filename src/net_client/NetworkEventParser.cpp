#include "NetworkEventParser.hpp"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace NetworkEventParser {

std::optional<std::variant<SoundEvent, GameLifecycleEvent>> parse(const std::string& raw) {
    try {
        json j = json::parse(raw);
        if (!j.is_object()) return std::nullopt;

        std::string type = j.value("type", std::string());
        if (type == "sound") {
            SoundEvent e;
            e.name = j.value("name", std::string());
            return e;
        }
        if (type == "lifecycle") {
            GameLifecycleEvent e;
            e.phase = j.value("phase", std::string());
            e.result = j.value("result", std::string());
            return e;
        }
        return std::nullopt;
    } catch (const json::exception&) {
        return std::nullopt;
    }
}

} // namespace NetworkEventParser
