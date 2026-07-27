#ifndef NET_CLIENT_DISCONNECT_STATUS_DESERIALIZER_H
#define NET_CLIENT_DISCONNECT_STATUS_DESERIALIZER_H
#include <optional>
#include <string>

// docs/tasks/graphics-networked-client-plan.md, Task H7: client-side mirror
// of server/GameStateSerializer.hpp's own DisconnectStatus - same per-color
// "milliseconds remaining before auto-resign" shape, parsed back out of the
// same periodic state-tick broadcast GameStateDeserializer.hpp already
// consumes (both whiteDisconnectMs/blackDisconnectMs live in that same JSON
// object). Deliberately a separate type/parser, not folded into
// GameSnapshot/GameStateDeserializer - see plan decision 3: which color's
// connection is currently down is connection-layer state, not game state,
// so it doesn't belong on the read-model Local Play's connection-less
// GameEngine also produces. HudView::compose() takes this as an additive,
// defaulted parameter (plan's own suggested shape) rather than GameSnapshot
// growing new fields.
struct DisconnectStatus {
    std::optional<int> whiteRemainingMs;
    std::optional<int> blackRemainingMs;
};

namespace DisconnectStatusDeserializer {

// std::nullopt for anything that isn't a state-broadcast frame (same
// has-a-"board"-key discriminator GameStateDeserializer::deserialize()
// uses) - malformed JSON, or any other message shape reachable on this
// connection. On a real state broadcast, each field is std::nullopt when
// that color isn't currently disconnected (absent key or JSON null),
// matching GameStateSerializer::DisconnectStatus's own default-constructed
// meaning on the server side.
std::optional<DisconnectStatus> deserialize(const std::string& raw);

}
#endif
