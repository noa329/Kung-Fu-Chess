#ifndef SERVER_DISCONNECT_TIMEOUT_H
#define SERVER_DISCONNECT_TIMEOUT_H

// Task D4: the pure "has this disconnected seat waited too long" decision,
// decoupled from real timers/sockets - same extraction pattern
// MatchmakingTimeout (D3) already established for the analogous "has this
// seeker waited too long" question. The real steady_timer glue
// (WebSocketServer) measures actual elapsed time since the disconnect (not
// the nominal 20000ms) and whether the seat is still marked disconnected,
// and hands both to this.
namespace DisconnectTimeout {

constexpr int kTimeoutMs = 20000;

// stillDisconnected matters because the player can reconnect (clearing the
// disconnected flag) in the window between when this timer was scheduled
// and when it fires - auto-resigning them anyway at that point would be
// wrong, even if elapsedMs has passed 20 seconds. (In practice
// WebSocketServer also cancels a reconnected seat's timer directly, same
// defense-in-depth relationship MatchmakingTimeout's stillQueued has with
// its own timer cancellation on match.)
inline bool shouldAutoResign(int elapsedMs, bool stillDisconnected) {
    return stillDisconnected && elapsedMs >= kTimeoutMs;
}

} // namespace DisconnectTimeout
#endif
