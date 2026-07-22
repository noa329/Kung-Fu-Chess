#ifndef SERVER_MATCHMAKING_TIMEOUT_H
#define SERVER_MATCHMAKING_TIMEOUT_H

// Task D3: the pure "has this seeker waited too long" decision, decoupled
// from real timers/sockets entirely - same extraction pattern every other
// networking-glue task in this codebase uses (ConnectionRegistry/A5,
// SessionManager/D1, MatchmakingQueue/D2). The real steady_timer glue
// (WebSocketServer) measures actual elapsed time (not the nominal 60000ms
// - the same "don't assume the nominal timer value is the real elapsed
// time" lesson A5's own tick-timing bug already established) and whether
// the seeker is still in MatchmakingQueue, and hands both to this.
namespace MatchmakingTimeout {

constexpr int kTimeoutMs = 60000;

// stillQueued matters because a seeker can be matched (and thus no longer
// queued) in the window between when their timer was scheduled and when
// it fires - timing them out anyway at that point would be wrong, even if
// elapsedMs has passed 60 seconds. (In practice WebSocketServer also
// cancels a matched seeker's timer directly, so this is defense in depth,
// not the only thing preventing a stale timeout.)
inline bool shouldTimeOut(int elapsedMs, bool stillQueued) {
    return stillQueued && elapsedMs >= kTimeoutMs;
}

} // namespace MatchmakingTimeout
#endif
