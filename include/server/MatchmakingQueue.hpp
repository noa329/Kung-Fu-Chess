#ifndef SERVER_MATCHMAKING_QUEUE_H
#define SERVER_MATCHMAKING_QUEUE_H
#include <algorithm>
#include <cstdlib>
#include <optional>
#include <utility>
#include <vector>

// Task D2: pure "who plays whom" pairing decision, decoupled from
// sockets/GameSession/SessionManager entirely - same extraction pattern
// SessionManager (D1)/ConnectionRegistry (A5) already established for
// testability. Templated on the seeker-id type so tests can use plain
// ints. Deliberately NOT wired into WebSocketServer yet - that lands
// together with Task D3's 60s timeout, since D3 needs this real queue to
// test expiry against anyway (see docs/tasks/server-phase-plan.md).
//
// enqueue() and tryMatch() are deliberately separate calls, not a single
// enqueue-that-auto-matches: the plan's own wording ("on each new enqueue
// (or on a periodic scan), check the whole queue") frames the trigger
// (enqueue-time vs. a periodic scan) as the caller's decision, not this
// class's job.
//
// Identity comparisons (remove()/contains()) use plain operator== - fine
// for the plain-int ids these tests use. websocketpp::connection_hdl (a
// std::weak_ptr<void>) has no operator==, so whatever wires this in later
// will need either a small locally-allocated seeker id instead of the raw
// hdl, or an owner_equal-style comparator (SessionManager's Compare
// template parameter solved the analogous ordering problem for
// std::map) - not solved here since it's not needed until this is
// actually wired in.
template <typename SeekerId>
class MatchmakingQueue {
public:
    void enqueue(const SeekerId& id, int rating) {
        queue_.push_back({id, rating});
    }

    // Scans the WHOLE queue (not just adjacent entries - two seekers can
    // be in range even with an out-of-range seeker enqueued between them)
    // for the first pair within kMaxRatingDiff of each other. Removes and
    // returns that pair, or std::nullopt (queue left unchanged) if no pair
    // is in range yet.
    std::optional<std::pair<SeekerId, SeekerId>> tryMatch() {
        for (size_t i = 0; i < queue_.size(); ++i) {
            for (size_t j = i + 1; j < queue_.size(); ++j) {
                if (std::abs(queue_[i].rating - queue_[j].rating) <= kMaxRatingDiff) {
                    SeekerId a = queue_[i].id;
                    SeekerId b = queue_[j].id;
                    // Erase j before i - erasing i first would shift j's index.
                    queue_.erase(queue_.begin() + static_cast<long>(j));
                    queue_.erase(queue_.begin() + static_cast<long>(i));
                    return std::make_pair(a, b);
                }
            }
        }
        return std::nullopt;
    }

    // Removes a seeker without matching them - Task D3's 60s timeout, or
    // a disconnect while queued (Task D4), will use this.
    bool remove(const SeekerId& id) {
        auto it = std::find_if(queue_.begin(), queue_.end(),
                                [&id](const Seeker& s) { return s.id == id; });
        if (it == queue_.end()) return false;
        queue_.erase(it);
        return true;
    }

    bool contains(const SeekerId& id) const {
        return std::find_if(queue_.begin(), queue_.end(),
                             [&id](const Seeker& s) { return s.id == id; }) != queue_.end();
    }

    size_t size() const { return queue_.size(); }

private:
    // "within ELO ±100" per the plan - inclusive at exactly 100.
    static constexpr int kMaxRatingDiff = 100;
    struct Seeker {
        SeekerId id;
        int rating;
    };
    std::vector<Seeker> queue_;
};
#endif
