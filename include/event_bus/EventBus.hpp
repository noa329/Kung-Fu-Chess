#ifndef EVENT_BUS_EVENT_BUS_H
#define EVENT_BUS_EVENT_BUS_H
#include "Events.hpp"
#include <cstddef>
#include <functional>
#include <utility>
#include <vector>

// Minimal typed pub/sub channel for one event payload type. Publishers call
// publish(); subscribers register a handler and get back a token they can
// later pass to unsubscribe() - this is what lets a future per-connection
// server-layer listener detach cleanly when a client disconnects, instead of
// living for the whole process like today's SoundManager singleton.
template <typename EventT>
class Signal {
public:
    using Handler = std::function<void(const EventT&)>;

    size_t subscribe(Handler handler) {
        size_t token = nextToken_++;
        handlers_.emplace_back(token, std::move(handler));
        return token;
    }

    void unsubscribe(size_t token) {
        for (auto it = handlers_.begin(); it != handlers_.end(); ++it) {
            if (it->first == token) {
                handlers_.erase(it);
                return;
            }
        }
    }

    void publish(const EventT& event) const {
        for (const auto& entry : handlers_) {
            entry.second(event);
        }
    }

private:
    size_t nextToken_ = 0;
    std::vector<std::pair<size_t, Handler>> handlers_;
};

// One Signal per event category. Publishers (GameEngine) and subscribers
// (SoundManager glue, later the server layer) depend on this header only -
// it has no dependency of its own on game_engine, renderer, or audio, so it
// stays a leaf in the layer graph.
struct EventBus {
    Signal<ScoreUpdatedEvent> onScoreUpdated;
    Signal<MoveLoggedEvent> onMoveLogged;
    Signal<SoundEvent> onSound;
    Signal<GameLifecycleEvent> onGameLifecycle;
};

#endif
