#ifndef SERVER_GAME_SESSION_H
#define SERVER_GAME_SESSION_H
#include "GameEngine.hpp"
#include "GameCommandParser.hpp"
#include "Logger.hpp"
#include <string>
#include <vector>

// Result of GameSession::handleCommand - covers only the validation this
// class is responsible for (is the sender actually the player they're
// claiming to command, is there a piece at the claimed square, does its
// color/letter match the command). Anything past that (shape/path/timing
// legality: illegal shape, blocked path, resting piece, pending move) is
// GameEngine's own existing silent-no-op behavior via select()/jump() -
// exactly the same as an illegal click through Controller today.
// handleCommand does not, and cannot, report those as errors, since
// GameEngine's public API has no return value for them.
struct CommandResult {
    bool ok;
    std::string error; // set only when ok == false, e.g. "ERROR PIECE_MISMATCH"
};

// Task B2: player-slot color assignment. First successful call gets
// White, second gets Black.
//
// Task E1: a third-and-later call no longer errors - it's a spectator.
// GameSession is deliberately the *one* place that decision lives (not
// duplicated between the matchmaking path and a room path): whether a
// session came from being matched or from a room's join-by-ID, both ever
// only call this same join(), so "1st=White, 2nd=Black, 3rd+=spectator"
// is enforced exactly once regardless of how the session was created.
// This is also why hasOpponent is always true for a spectator join - the
// 3rd+ join can't happen until both seats are already filled.
//
// Task D3: authentication is no longer part of this at all.
// GameSession::handleJoin(username, password) (Task C3) is gone -
// authentication now happens exactly once, at login time, directly via
// WebSocketServer's own AuthService, *before* any GameSession exists to
// call it on (matchmaking/room-join is what decides which already-
// authenticated username ends up in which session, and only then is one
// created). So GameSession no longer knows about AuthService/passwords at
// all - see join() below, which takes only an already-authenticated
// username.
struct JoinResult {
    bool ok;
    char color;       // 'w' | 'b' | 's' (spectator), meaningful only when ok == true
    bool hasOpponent; // true when both seats were already filled before
                      // this call - always true for a spectator join (E1),
                      // true for the 2nd seat assignment, false for the 1st
    std::string error; // set only when ok == false - currently unreachable
                        // (join() never rejects a call any more, see E1's
                        // comment above), kept for API stability/symmetry
                        // with the rest of this codebase's Result structs
};

// Owns one GameEngine (one concurrent game = one GameSession = one
// isolated GameEngine + EventBus, per the event_bus design). Turns an
// already-parsed ParsedCommand (Task A2) into the corresponding
// GameEngine calls. Connection/socket handling is a later task (A5) -
// this class deliberately knows nothing about networking yet.
//
// Subscribes to engine_.events() (onMoveLogged/onScoreUpdated/
// onGameLifecycle/onSound) in the constructor, always - but every
// subscription is gated on logger_ being non-null, so a default-
// constructed GameSession (used throughout the existing test suite)
// stays silent. attachLogger() lets the composition root (WebSocketServer)
// opt a real session into logging without needing a second constructor
// overload or breaking GameSession()'s existing default-constructibility.
// Logger itself is owned externally (WebSocketServer, shared across every
// session since SessionManager/Task D1) - not per-session, so multiple
// concurrent sessions can log to one shared file.
class GameSession {
    GameEngine engine_;
    Logger* logger_ = nullptr;
    bool whiteJoined_ = false;
    bool blackJoined_ = false;
    std::string whiteUsername_;
    std::string blackUsername_;
    // Task E1: 3rd+ joiners. No cap, no dedup by username (two joins of
    // the same username - e.g. two browser tabs - are two independent
    // spectator entries) - nothing in the deck asks for either, and nothing
    // here currently needs to distinguish them from each other individually.
    std::vector<std::string> spectatorUsernames_;
    // Task D4: per-seat connection status. Defaults true - a seat only
    // ever becomes disconnected via an explicit markDisconnected() call
    // from WebSocketServer's close handler, never implicitly. Deliberately
    // plain bools, not a shared "ConnectionState" enum: this is
    // server-layer connection bookkeeping, not one of the board/animation
    // states the project's strings-not-enums convention actually governs.
    bool whiteConnected_ = true;
    bool blackConnected_ = true;

    void logEvent(const std::string& message);

public:
    GameSession();

    GameEngine& engine() { return engine_; }
    void attachLogger(Logger& logger) { logger_ = &logger; }

    // Task E2: `username` is the identity of the connection that actually
    // sent this command (resolved by WebSocketServer from the sending
    // hdl, same as everywhere else GameSession takes a username instead
    // of a connection_hdl - see the class comment above). `cmd.color` is
    // only what the wire text *claims* - GameCommandParser has no way to
    // know who sent it, so it can't be trusted on its own. This checks
    // `colorOf(username)` (the seat this specific connection actually
    // holds, established once at join() and never re-derived from the
    // command itself) against `cmd.color` *before* any board-state check:
    // a spectator (or an unjoined username) is rejected outright
    // (`ERROR NOT_A_PLAYER`), and a real player claiming the *other*
    // color - e.g. Black's own connection sending a White-claiming
    // command, which would otherwise sail through the board check since
    // the claimed color and the board's actual color could both
    // legitimately be White - is rejected too (`ERROR NOT_YOUR_COLOR`).
    // Without this, identity was never checked at all: only "does the
    // claimed color match the board", which any connection could satisfy
    // by simply claiming whichever color was actually on the square.
    CommandResult handleCommand(const std::string& username, const ParsedCommand& cmd);
    // Task G3: voluntary resign. Unlike handleCommand, there's no claimed
    // color to reconcile against the board - colorOf(username) alone
    // already tells us which seat (if any) this connection actually
    // holds, so a resign command carries no color/piece/square at all on
    // the wire. Same ERROR NOT_A_PLAYER rejection as handleCommand for a
    // spectator/unjoined username; otherwise resigns colorOf(username)'s
    // seat. GameEngine::resign() already no-ops if the game is already
    // over, so a stray double-resign (or a resign after the game ended
    // some other way) is safe without any extra guard here.
    CommandResult handleResign(const std::string& username);
    // Assigns a role (White/Black/spectator) to an already-authenticated
    // username - see the class comment above for why authentication
    // itself isn't this class's job (or even reachable from here) any
    // more, and why this is the one place both the matchmaking path and a
    // room's join-by-ID path both call, rather than each having its own
    // role-assignment logic.
    JoinResult join(const std::string& username);

    // Task D4: which seat (if any) a username occupies in this session -
    // '\0' if it's neither whiteUsername_ nor blackUsername_ - notably
    // '\0' for a spectator too, not just an absent username: colorOf()'s
    // only consumer (WebSocketServer's disconnect-countdown machinery)
    // needs "does this username hold a resignable seat", and a spectator
    // doesn't, same as someone not in this session at all. Use
    // isSpectator() below to tell those two '\0' cases apart. Lets
    // WebSocketServer turn "this hdl closed" (which it only knows a
    // username for, via its own authenticatedUsers_) into "this session's
    // white/black seat just disconnected" without GameSession needing to
    // know about connection_hdl at all.
    char colorOf(const std::string& username) const;
    // Task E1: true if `username` joined as a spectator (3rd+ join) -
    // see colorOf()'s comment above for why this is a separate query
    // rather than folded into colorOf() itself.
    bool isSpectator(const std::string& username) const;
    // The username occupying `color`'s seat, or "" if that seat isn't
    // filled yet. Used by WebSocketServer's reconnect ack to report the
    // opponent's name the same way handleMatch()'s original "joined"
    // message does.
    std::string usernameFor(char color) const;

    void markDisconnected(char color);
    void markReconnected(char color);
    bool isConnected(char color) const;
};
#endif
