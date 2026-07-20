# Kung Fu Chess — server phase: full plan

## How to use this file

Same convention as `full-graphics-buildout-plan.md`: work through phases
**in order**, one small commit per task (tests-first where the task has
pure logic to test), confirm the doctest suite still passes before moving
on. Each phase after Phase A generalizes something Phase A built as a
hardcoded special case (one session, two clients) — expect touching earlier
files again, not just adding new ones.

This plan covers everything in the instructor's slide deck *except* the
EventBus (`include/event_bus/`), which is already built and merged — see
git log. `CLAUDE.md`'s architecture ground rules apply to all of this; the
layers described below are new siblings of the 8 existing ones, not
replacements.

Stop and confirm with me before starting a phase whose "Open questions"
section isn't fully resolved yet — don't guess and build on the guess.

---

## Decisions already confirmed (don't re-litigate these)

- **WebSocket library:** `websocketpp` + standalone Asio, fetched via
  CMake `FetchContent` (see the CMake/build-system section below — this
  superseded an earlier plan to hand-vendor both under `third_party/`).
  Server builds under **MSYS2/ucrt64/g++, via CMake+Ninja — not MSVC**.
  The server has no OpenCV/rendering dependency, so tying it to the
  graphics build's MSVC toolchain would be pulling in a dependency it
  doesn't need, and OpenCV's prebuilt MSVC-ABI `.lib` is the *only* reason
  that build needs MSVC in the first place.
- **Build system:** a new, separate `server/CMakeLists.txt` (own CMake
  project, own `server/build/`) — not merged into
  `kungfu-graphics/cpp/CMakeLists.txt`, and not a Makefile target either
  (superseded the original plan). Kept separate from the graphics
  `CMakeLists.txt` specifically because one CMake project can't cleanly
  target MSVC for one executable and MinGW for another in the same
  configure. The existing Makefile/`run_tests.exe`/doctest workflow is
  untouched by any of this — see "Dual-compilation" below for how
  `server/`'s pure-logic code still gets doctest coverage through it.
- **Command parser location:** new `server/` layer, not `text_io/` —
  `text_io` stays scoped to the `Board:` grid format; the wire-protocol
  grammar is the server's concern.
- **JSON:** `nlohmann::json`, vendored under `third_party/` (committed,
  like `miniaudio/`) rather than `FetchContent`-only — unlike
  `websocketpp`/Asio, `GameStateSerializer` (A4) needs doctest coverage
  under the existing Makefile flow, which `FetchContent` can't reach (see
  the CMake/build-system section below for the full reasoning, same for
  the sqlite amalgamation).
- **Shell client gameplay rendering:** text-only. The new shell client
  reuses `BoardPrinter` to print the board from each state broadcast and
  reads command strings from stdin. It never touches OpenCV. Teaching the
  *graphics* binary to speak the network protocol stays a separate,
  unscoped future task.
- **Password hashing:** SHA-256 + per-user random salt, via a small
  vendored single-header implementation (exact source still needs picking
  — see open questions for Phase C).
- **Phase A verification:** server-only; proved out with an external WS
  test client (see Task A0 below), not the shell client (which doesn't
  exist until Phase B).

---

## Overall architecture

### New layers (siblings of the existing 8, not replacements)

```
server/                          — NEW top-level CMake project (own CMakeLists.txt, own build/)
    CMakeLists.txt                    — FetchContent for asio/websocketpp (later: nlohmann_json/sqlite3)
    main.cpp                          — the real accept-loop entry point, CMake-only, not dual-compiled
                                         (mirrors kungfu-graphics/cpp/src/main.cpp not being part of run_tests.exe)

include/server/          src/server/      — dual-compiled: Makefile (run_tests.exe, doctest) AND
                                             server/CMakeLists.txt both compile these
    GameSession.hpp            — owns one GameEngine + up to 2 player connections + spectators
    SessionManager.hpp         — connection -> GameSession routing (from Phase D on; Phase A/B hardcode one session)
    GameCommandParser.hpp      — "WQe2e5" -> ParsedCommand (pure, unit-testable, no networking)
    GameStateSerializer.hpp    — GameSnapshot subset -> JSON (pure, unit-testable)
    AuthService.hpp            — (Phase C) login/register message handling
    MatchmakingQueue.hpp       — (Phase D)
    RoomRegistry.hpp           — (Phase E)
    WebSocketServer.hpp/.cpp   — websocketpp/asio glue: accept, route messages, tick timer.
                                  NOT dual-compiled (needs websocketpp/asio headers Makefile builds
                                  don't have) - added to a new SERVER_ONLY_SRC exclusion list in the
                                  Makefile in Task A1, mirroring the existing OPENCV_ONLY_SRC pattern
                                  used for the renderer's OpenCV-dependent files.

include/persistence/     src/persistence/
    Database.hpp                — thin SQLite C-API wrapper (open/exec/query), no game/network knowledge
    UserRepository.hpp          — users table CRUD + rating updates

include/logging/         src/logging/
    Logger.hpp                   — shared by server/ AND the shell client; file + console sinks

client/cli/
    main.cpp                     — NEW program: shell login/lobby/gameplay client (Phase B on)

third_party/
    miniaudio/                  — vendored single header, committed (unchanged)
    README.md                    — documents both the committed and FetchContent-fetched dependencies
```

Asio and `websocketpp` are **not** under `third_party/` at all any more —
see "CMake migration" correction below. **`nlohmann::json` and sqlite are
a different case, and stay vendored under `third_party/` (committed,
like `miniaudio/`), not `FetchContent`-only:** unlike `websocketpp`/Asio
(only ever touched by `server/main.cpp`'s accept loop, which is
CMake-only and never dual-compiled), `GameStateSerializer` (A4) and
`persistence/` (C1/C2) are *dual-compiled* pure-logic code that needs
doctest coverage under the existing Makefile/`run_tests.exe` flow —
`FetchContent` only populates content inside `server/build/`, which the
Makefile has no equivalent mechanism to reach. So `nlohmann::json`
(single header) and the sqlite amalgamation get vendored directly, same
reasoning as `miniaudio.h`, and `server/CMakeLists.txt` references those
same `third_party/` paths rather than fetching its own separate copies.

**First correction (found during Task A0):** Asio was originally going to
be pacman-installed. Turned out not to work — the pacman
`mingw-w64-ucrt-x86_64-asio` package is 1.38.0, which has fully removed
the deprecated `io_service`/`io_service::strand`/`expires_from_now` API
that `websocketpp` 0.8.2 (its latest tagged release, last updated ~2018)
hard-depends on in its Asio transport — confirmed by a real compile
failure (`'io_service' in namespace 'websocketpp::lib::asio' does not
name a type`, `m_strand->wrap(...)`: "base operand of '->' is not a
pointer", etc.), not a guess. **Fix:** pin Asio at **1.18.2**, from the
upstream `chriskohlhoff/asio` tag rather than pacman.

**Second correction (also Task A0, this one from the instructor's
build-system guidance rather than a technical failure):** the server
build moved from a Makefile target to its own CMake project
(`server/CMakeLists.txt`), staying on the MSYS2/ucrt64 toolchain via
CMake+Ninja rather than MSVC. Once on CMake, `websocketpp` and Asio moved
from hand-vendoring under `third_party/` to CMake `FetchContent`, pinned
at the exact same versions (`asio-1-18-2`, `0.8.2`) the manual vendoring
already validated — see `third_party/README.md`'s "History" section for
the full before/after. The manually-vendored copies (`third_party/websocketpp/`,
committed; `third_party/asio/`, gitignored) are gone from both git and
disk.

One more MinGW-specific wrinkle, unaffected by the CMake migration:
`websocketpp`'s `common/thread.hpp` has a blanket rule that disables its
C++11 `<thread>` path on any MinGW target (`__MINGW32__`/`__MINGW64__`
defined), falling back to a `<boost/thread.hpp>` include we don't have —
a leftover from when older MinGW lacked real `std::thread` support; not
true of the current MSYS2/ucrt64 toolchain. `server/CMakeLists.txt` sets
`-D_WEBSOCKETPP_CPP11_THREAD_` via `target_compile_definitions` to force
the modern path.

Verified end-to-end: the same echo server (now living permanently at
`server/main.cpp`, not a throwaway) compiles clean via
`cmake -S server -B server/build -G Ninja` + `cmake --build server/build`
and round-trips a real WebSocket handshake + text frame against
`scripts/ws_test_client.py`.

### Per-session `GameEngine`, and how the EventBus fits in

Each concurrent game = one `GameEngine` instance = one isolated `EventBus`
(this is exactly why the EventBus task made `GameEngine` own its bus
instead of a singleton — this is the payoff). `GameSession` owns the
`GameEngine` and subscribes to its `events()` at construction time, the
same pattern already used for `SoundManager`/`HudView` in the graphics
`main.cpp`.

**Two different jobs, don't conflate them:**
- **State sync to clients** (the deck's "send back resulting game state"):
  driven by the ~16ms tick, not by individual events. Every tick:
  `engine.wait(dt)` → serialize `engine.snapshot()` → broadcast to every
  connection in the session (players + spectators). Simple, robust, matches
  "both clients see the result" literally.
- **Server-side logging** (deck item 6): driven by the EventBus.
  `GameSession` subscribes `Logger` calls to `onMoveLogged`/
  `onScoreUpdated`/`onGameLifecycle`/`onSound` — this is what actually
  uses the EventBus's pub/sub nature (discrete "this happened" logging),
  as opposed to the continuous tick-driven broadcast.

Don't try to make the EventBus *also* drive the broadcast (e.g. "broadcast
only on `onMoveLogged`") — travel-time moves need continuous
`moveProgress`-free-but-still-ticking state (captures resolving,
short/long rest expiring) that isn't tied to a discrete event, so the tick
stays the source of truth for sync. This mirrors the plan-approved
decision from the EventBus task: events are for discrete triggers, not a
replacement for the continuous read-model.

### Concurrency model

**Recommendation (not yet asked as a question — flagging so you can
object): single-threaded asio `io_context`, with the ~16ms tick implemented
as a chained `asio::steady_timer` (`async_wait` → tick → reschedule) on
that same thread**, not a separate OS thread. This keeps every
`GameSession`/`GameEngine` touched from exactly one thread, so no mutexes
anywhere in `server/`. "Independent" (from your Phase-A confirmation) means
independent of *incoming client messages* — the timer fires on its own
schedule regardless of whether a command just arrived — not independent as
in "a separate OS thread." Flag if you actually want a thread-per-session
model (there's no obvious reason to, at this scale — single process,
handful of concurrent games).

### What stays untouched

`Controller`'s pixel→`Position` conversion is irrelevant here — the server
never sees pixels, it goes straight from a parsed algebraic square to
`Position` and calls `GameEngine::select()`/`jump()` directly. `Controller`
remains graphics-input-only, no server dependency on it at all.

---

## SQLite schema (Phase C)

```sql
CREATE TABLE users (
    id            INTEGER PRIMARY KEY AUTOINCREMENT,
    username      TEXT NOT NULL UNIQUE,
    password_hash TEXT NOT NULL,
    password_salt TEXT NOT NULL,
    rating        INTEGER NOT NULL DEFAULT 1200,
    created_at    INTEGER NOT NULL  -- unix ms
);
```

No match-history table — the deck only asks for a live `rating` field
adjusted after each game, not a history/profile screen. Flag if you want
one anyway (cheap to add now, more annoying to retrofit once games are
being played against the live schema).

DB file default location: `data/kungfu_chess.db`, new gitignored `data/`
folder (runtime-generated, not source) — flag if you want it elsewhere.

`UserRepository` tests use SQLite's `:memory:` database — no disk I/O
needed for tests, same trick as the existing doctest suite's
`istringstream`-based text_io tests.

---

## Task breakdown

Each task = one commit (or a couple, if it splits naturally like the
EventBus task did). "Depends on" references earlier task IDs. Pure-logic
tasks get doctest coverage; networking-glue tasks get a documented manual
verification procedure instead (real sockets aren't practical to doctest,
same reasoning as the graphics build's headless-PNG-probe convention for
OpenCV rendering).

### Phase A — Basic WebSocket server (deck item 1)

| Task | What | Depends on | Tests |
|---|---|---|---|
| **A0** ✅ | **Done, twice.** Originally: hand-vendored `websocketpp`+Asio under `third_party/`. Then migrated to CMake `FetchContent` (`server/CMakeLists.txt`, pinned at the same versions) once the instructor's build-system guidance called for CMake. Also wrote `scripts/ws_test_client.py` (stdlib-only hand-rolled RFC6455 client, no pip deps) reused for every later phase's manual verification. | — | Echo round-trip via `ws_test_client.py`, verified against both the original manually-vendored build and the final CMake/FetchContent build. |
| **A1** | Partially done as a side effect of A0's CMake migration: `server/main.cpp` already accepts a connection and echoes what it receives (no game logic). Remaining for A1 proper: pull in the reused engine layers (`model`/`movement_rules`/`rule_engine`/`real_time_arbiter`/`game_engine`/`controller`/`event_bus`) as CMake sources/include dirs, and add the `SERVER_ONLY_SRC` exclusion to the Makefile so `include/server/`+`src/server/`'s *pure* pieces (once A2-A6 add them) stay dual-compiled without dragging `websocketpp`/Asio into `run_tests.exe`. | A0 | Manual: connect with the script, send text, see it echoed (already passing). |
| **A2** | `GameCommandParser` — pure parser. `"WQe2e5"` → `ParsedCommand{color, pieceLetter, from, to}` for moves; a distinct leading token (design decision — pick and document, e.g. a `J` prefix: `"JWPe2"` for a jump) for jumps. Malformed input → an explicit parse-error result, not an exception. Needs a new algebraic→`Position` helper (the inverse of `GameEngine`'s private `squareName()`) — lives alongside the parser since only the server needs it. | — | doctest: valid move, valid jump, short/garbage string, bad color char, bad square (`"WQz9z9"`), empty string. |
| **A3** | `GameSession::handleCommand(ParsedCommand)` — looks up the board cell at `from` via the session's `GameEngine`, validates color+piece letter against what's actually there (**fail on mismatch**, per your decision), then calls `select(from); select(to);` for moves or `jump(from)` for jumps. Returns a success/error result (error reason for a rejected/malformed command). | A2 | doctest against a real `GameEngine` instance (same pattern as `test_game_engine.cpp`): correct command schedules the right move; piece-letter mismatch is rejected *without* mutating engine state; malformed command produces an error result. |
| **A4** | `GameStateSerializer` — `GameSnapshot` subset → JSON via `nlohmann::json`: board tokens, `cellStates`, scores, move history, `gameOver`/`result`. **Open question below** on whether `captureFlashes` belongs in this subset. | — | doctest: serialize a known snapshot, check the JSON string/parsed object has exactly the expected fields and values. |
| **A5** | Wire it together: `WebSocketServer` + `GameSession` for exactly one hardcoded session, up to 2 connections (3rd rejected outright, per your confirmation). Inbound text message → A2 parse → A3 execute → broadcast. Independent ~16ms `steady_timer` tick → `engine.wait(dt)` → broadcast to both. | A1, A3, A4 | Extract anything logic-only (e.g. "given N active connections, does a new one get accepted or rejected" as a pure function/small class independent of real sockets) into doctest. Everything else: manual procedure via `ws_test_client.py` — two connections, send `"WQe2e5"`, confirm both receive updated state; 3rd connection gets rejected. Document the exact manual steps in this task's commit message or a short companion note. |
| **A6** | `include/logging/Logger.hpp` (shared utility, file + console sinks) + wire `GameSession` to subscribe it to `onMoveLogged`/`onScoreUpdated`/`onGameLifecycle`/`onSound` for structured server-side game-event logs. This is a slice of deck item 6, pulled forward because it's small and the natural place to plug into the EventBus subscriptions already designed for exactly this. | A5 | doctest: `Logger` formats a line correctly given known inputs (inject a fake sink/stream). Manual: confirm log lines appear during the A5 manual test. |

### Phase B — Home screen basic (deck item 2)

| Task | What | Depends on | Tests |
|---|---|---|---|
| **B1** | `client/cli/main.cpp` skeleton: connects to the server via WS, no protocol yet — just a connectivity smoke test (mirrors A1's approach, but this time it's a real deliverable program, not throwaway). | A5 | Manual: run it against the running server, confirm connection. |
| **B2** | Server-side "join" message: `{ "type": "join", "username": "..." }` → assign White (1st joiner) / Black (2nd) / reject a 3rd (already enforced structurally by A5's 2-connection cap, now needs the username attached to the assignment and broadcast to both: "you are White", "opponent connected: <name>"). | A5, A4 (JSON) | doctest for the assignment logic itself (given join order, what color comes back) as a pure function decoupled from sockets, same extraction pattern as A5. |
| **B3** | Wire `client/cli`: prompt for username, send join, print assigned color + a "waiting for opponent" state until the second join arrives. | B1, B2 | Manual: two instances of the CLI client, confirm White/Black assignment matches join order. |
| **B4** | Wire `client/cli`'s gameplay loop: on each state broadcast, print the board via `BoardPrinter`; read a line from stdin, forward it verbatim as the command string to the server (client doesn't need its own parser — A2 already lives server-side and is the source of truth). | B3, A5 | Manual: play a full move/jump/capture sequence between two CLI clients, confirm both see consistent board state. |

### Phase C — Home screen upgraded (deck item 3)

| Task | What | Depends on | Tests |
|---|---|---|---|
| **C1** | `persistence/Database` — thin SQLite C-API wrapper (open, exec, prepared-statement query helpers). Vendor sqlite3 amalgamation under `third_party/sqlite/` (committed, like `miniaudio/` — needed by both the Makefile build, for doctest coverage, and `server/CMakeLists.txt`, which references this same path rather than fetching its own copy). | — | doctest against an in-memory (`:memory:`) database: create table, insert, query round-trip. |
| **C2** | `persistence/UserRepository` — users table CRUD + rating read/update. Vendor the chosen SHA-256 implementation (open question below), add salt-generation + hash-and-compare helpers. | C1 | doctest: create user, find by username, wrong password rejected, rating update persists. All against `:memory:`. |
| **C3** | `server/AuthService` — login/register message schema and handling, wired into `GameSession`'s join flow in place of B2's username-only assignment. | C2, B2 | doctest for the auth *decision* logic (given a stored user + a login attempt, accept/reject) decoupled from the socket layer. Manual: full login round-trip via `client/cli`. |
| **C4** | Wire `client/cli`'s login prompt to ask for username **and** password; handle the reject/accept responses. **Open question below:** auto-register on first-ever username, or explicit separate register vs. login commands? | C3, B3 | Manual. |
| **C5** | ELO rating update on game end: subscribe to `onGameLifecycle("end", ...)` (same EventBus hook A6 already established the pattern for), compute both players' new ratings via standard ELO, persist via `UserRepository`, include updated ratings in the next state broadcast. **Open question below:** K-factor value (deck doesn't specify). | C2, A6 | doctest: known before-ratings + known result → known after-ratings, check against a hand-computed ELO example. |

### Phase D — Matchmaking (deck item 4)

| Task | What | Depends on | Tests |
|---|---|---|---|
| **D1** | `server/SessionManager` — generalizes A5's hardcoded single session into N concurrent sessions, connection→session routing. **This is a real refactor of A5's `WebSocketServer`, not purely additive** — flagging now so it's not a surprise later. | A5 | doctest for the routing logic (pure, given a connection-id→session-id map). |
| **D2** | `server/MatchmakingQueue` — "Play" enqueues a seeker; on each new enqueue (or on a periodic scan), check the whole queue for any pair within ELO ±100 of each other (symmetric check, not just adjacent-in-queue-order); on match, create a new `GameSession` via D1. | D1, C2 (needs ratings) | doctest: seed a queue with known ratings, confirm correct pairing (and non-pairing when nobody's in range). |
| **D3** | 1-minute timeout: a `steady_timer` per seeker; if unmatched after 60s, dequeue and send "can't find opponent". | D2 | doctest for the timeout *decision* (given elapsed time, still-queued state) as a pure check; manual for the real timer firing. |
| **D4** | Disconnect-mid-game handling: detect socket close → start a 20s countdown → broadcast the countdown each tick to the remaining client → auto-resign (reuse the existing `onGameLifecycle("end", ...)` path) if it elapses. **Open question below:** is reconnect within the 20s window supported at all, or is this strictly countdown-then-resign with no way back in? | D1, A6 | Manual (needs a real disconnect to trigger) — document the exact repro steps. |

### Phase E — Rooms (deck item 5)

| Task | What | Depends on | Tests |
|---|---|---|---|
| **E1** | `server/RoomRegistry` — create (generate room ID, format TBD — open question below), join-by-ID, tracks 1st joiner = White, 2nd = Black, 3rd+ = spectators. | D1 | doctest: create/join/spectator-assignment logic, pure. |
| **E2** | Spectator enforcement: `GameSession`/A3's command handler rejects move/jump commands from a spectator connection before they ever reach `GameEngine`. | E1, A3 | doctest: spectator-submitted command is rejected without mutating engine state. |
| **E3** | Wire `client/cli`'s "Room" flow: Create/Join/Cancel prompt, room ID displayed at the top of the text UI. | E1, B4 | Manual: three CLI clients — two players, one spectator — confirm spectator sees state but can't move. |

### Phase F — Logging completeness (deck item 6, remaining slice)

| Task | What | Depends on | Tests |
|---|---|---|---|
| **F1** | Wire `client/cli` to use the same `logging/Logger` (A6) for its own activity — connect, commands sent, state received, errors — to a local client-side log file. | A6, B1 | Manual: confirm log file populated during a normal play session. |
| **F2** | Audit pass: confirm every server-side branch introduced across Phases A–E (auth failure, malformed command, matchmaking timeout, disconnect/auto-resign, room create/join/reject) has a log line. Mostly gap-filling, not new architecture. | all of the above | N/A — review pass. |

---

## Consolidated open questions

Flagging everything I'd guess on otherwise — please resolve before the
relevant phase starts (not necessarily all before Phase A, since most of
these belong to later phases):

1. **A2 — jump command syntax.** I proposed a distinct leading token (e.g.
   `"J"` prefix) since jump maps to a different `GameEngine` call than
   move. Confirm the exact convention (leading char, or some other
   marker) so I document it consistently before writing the parser.
2. **A4 — does the reduced state payload include `captureFlashes`?**
   You confirmed board tokens/`cellStates`/scores/move history/`gameOver`/
   `result`. `captureFlashes` is arguably part of "what the client should
   show," distinct from the per-frame-interpolation fields (`moveProgress`,
   `moveTargets`) that are clearly render-loop-only. Include it or not?
3. **C2 — which specific SHA-256 implementation to vendor?** "A small
   single-header public-domain SHA-256" is a category, not a pinned
   source — I'll need you to either name one or approve me picking a
   specific well-known one (e.g. Brad Conte's `sha256.h`/.c, public domain)
   when C2 starts.
4. **C4 — auto-register vs. explicit register/login.** Does a
   never-seen-before username on the login screen get silently created
   (auto-register), or does the deck imply separate explicit "register"
   and "login" actions?
5. **C5 — ELO K-factor.** Deck doesn't specify one. Common defaults are
   16–32; I'd default to 32 (standard for less-established/casual play)
   unless you want a specific value.
6. **D4 — is mid-game reconnect supported at all** within the 20s
   auto-resign countdown, or is it strictly one-way (disconnect starts the
   clock, nothing stops it short of the game already having ended)?
7. **E1 — room ID format/length.** Deck doesn't specify. I'd default to a
   short random alphanumeric code (e.g. 6 characters) unless you want
   something else (numeric-only, longer, human-pronounceable words, etc).
8. **Concurrency model** (see architecture section above) — single-threaded
   `io_context` with a chained timer, recommended but not yet explicitly
   confirmed by you.
9. **Should there be a match-history table** beyond the live `rating`
   field, for a future profile/history screen? Not required by the deck
   items given, cheap to add now, more annoying to retrofit later.
