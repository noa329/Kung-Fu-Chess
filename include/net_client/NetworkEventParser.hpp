#ifndef NET_CLIENT_NETWORK_EVENT_PARSER_H
#define NET_CLIENT_NETWORK_EVENT_PARSER_H
#include "Events.hpp"
#include <optional>
#include <string>
#include <variant>

// docs/tasks/graphics-networked-client-plan.md, Task H2: parses Task G4's
// discrete event-push messages ({"type":"sound","name":...} /
// {"type":"lifecycle","phase":...,"result":...}, see
// src/server/GameSession.cpp's constructor for the exact wire shapes) back
// into the exact same SoundEvent/GameLifecycleEvent types GameEngine's own
// EventBus already publishes locally (event_bus/Events.hpp) - reusing them
// rather than inventing a parallel wire-event type, the same "reuse the
// existing type" call GameStateDeserializer.hpp makes for GameSnapshot.
// This is what lets Task H6 feed a networked message straight into
// SoundManager::playSound()/HudView::playEndAnimation() exactly the way the
// local composition root's own EventBus subscribers already do (see
// kungfu-graphics/cpp/src/main.cpp's runLocalGame()) - same consumer, two
// different trigger sources.
namespace NetworkEventParser {

// std::nullopt for anything that isn't a recognized "type":"sound" or
// "type":"lifecycle" message: malformed JSON, a state-broadcast frame (has
// "board", no "type" - GameStateDeserializer's job, not this parser's), or
// any other "type"-tagged message reachable on this connection (join/
// joined/room_created/etc - Task H3b's concern).
std::optional<std::variant<SoundEvent, GameLifecycleEvent>> parse(const std::string& raw);

}
#endif
