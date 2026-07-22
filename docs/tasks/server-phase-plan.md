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
    CMakeLists.txt                    — FetchContent for asio/websocketpp; references third_party/nlohmann
                                         directly (vendored, not FetchContent - see below) and will do the
                                         same for sqlite3 (later, Task C1)
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

### Phase A — Basic WebSocket server (deck item 1) ✅ complete (A0–A6)

| Task | What | Depends on | Tests |
|---|---|---|---|
| **A0** ✅ | **Done, twice.** Originally: hand-vendored `websocketpp`+Asio under `third_party/`. Then migrated to CMake `FetchContent` (`server/CMakeLists.txt`, pinned at the same versions) once the instructor's build-system guidance called for CMake. Also wrote `scripts/ws_test_client.py` (stdlib-only hand-rolled RFC6455 client, no pip deps) reused for every later phase's manual verification. | — | Echo round-trip via `ws_test_client.py`, verified against both the original manually-vendored build and the final CMake/FetchContent build. |
| **A1** ✅ | `server/CMakeLists.txt` now compiles the reused engine layers (`model`/`movement_rules`/`rule_engine`/`real_time_arbiter`/`game_engine`/`controller`, plus `event_bus`'s include dir - header-only, no `.cpp`) into `kungfu_server`, mirroring `kungfu-graphics/cpp/CMakeLists.txt`'s `ENGINE_SOURCES` glob pattern (minus `audio/` - no sound on the server - and minus `renderer/`/`text_io/`/`text_test_runner/` - graphics- or text-protocol-specific). `main.cpp` proves the linkage at startup: constructs a real `GameEngine`, runs one move through it, prints the resulting board - not wired into WS message handling yet, that's A2/A3. The `SERVER_ONLY_SRC` Makefile exclusion is deferred to whichever of A2-A6 first creates a `src/server/*.cpp` file that touches `websocketpp`/Asio directly (`WebSocketServer.cpp`) - nothing to exclude yet since `include/server/`/`src/server/` don't exist yet. | A0 | Manual: engine linkage proof prints the correct post-move board on startup; echo round-trip still passes. `run_tests.exe` unaffected (79/79, confirmed after this change). |
| **A2** ✅ | `GameCommandParser` (`include/server/GameCommandParser.hpp` + `src/server/GameCommandParser.cpp`) — pure parser, dual-compiled (Makefile + `server/CMakeLists.txt`, no `websocketpp`/Asio dependency). Full grammar decided and documented as a design-decision comment in the header: `"WQe2e5"` (`<Color><Piece><From><To>`) for moves, `"JWPe2"` (`J<Color><Piece><Square>`) for jumps — confirmed `J`-prefix convention. Casing is strict (uppercase color/piece/`J`, lowercase file letter) - a deliberate simplicity choice. Squares assume a fixed 8×8 board (wire commands only ever address a real game, unlike `GameEngine`'s own variable-size-board tests). Error taxonomy mirrors `BoardParser`'s `"ERROR <REASON>"` style: `MALFORMED_COMMAND`, `INVALID_COLOR`, `INVALID_PIECE`, `INVALID_SQUARE`. | — | 14 doctest cases: valid move, valid jump, black-color normalization (both), empty string, too-short/too-long for both move and jump, bad color char, lowercase rejected, bad piece letter, bad origin/destination/jump square. |
| **A3** ✅ | `GameSession` (`include/server/GameSession.hpp` + `src/server/GameSession.cpp`) — owns one `GameEngine`. `handleCommand(ParsedCommand)` looks up the board cell at `from` via `snapshot()`, validates color **and** piece letter against what's actually there (fail on mismatch: `ERROR NO_PIECE_AT_SQUARE` / `ERROR COLOR_MISMATCH` / `ERROR PIECE_MISMATCH`), then calls `select(from); select(to);` for moves or `jump(from)` for jumps. **Scope boundary worth remembering:** `CommandResult` only ever reports *this* validation failing — shape/path/timing legality (illegal shape, blocked path, resting piece, pending move) stays `GameEngine`'s existing silent-no-op via `select()`/`jump()`, exactly like an illegal click through `Controller` today; there's no way to report those as errors since `GameEngine`'s public API has no return value for them. Dual-compiled, no `websocketpp`/Asio dependency. | A2 | 7 doctest cases against a real `GameEngine` instance: correct move schedules and resolves; correct jump schedules; empty-square/color-mismatch/piece-mismatch/out-of-bounds all rejected *without* mutating engine state (board tokens **and** `selected` checked unchanged); a validation-passed-but-GameEngine-declines case (same-square move) confirmed as `ok == true` with no board change - documents the scope boundary above as an executable test, not just a comment. |
| **A4** ✅ | `GameStateSerializer` (`include/server/GameStateSerializer.hpp` + `src/server/GameStateSerializer.cpp`) — `GameSnapshot` subset → JSON via `nlohmann::json`: `board`, `cellStates`, `whiteScore`/`blackScore`, `whiteMoves`/`blackMoves` (each `{atMs, color, notation}`), `gameOver`/`result`. **`captureFlashes` confirmed excluded** — same render-loop-only category as `moveProgress`/`moveTargets`/`selected`; the text-only shell client has nothing to show a flash with. `nlohmann::json` vendored under `third_party/nlohmann/` (dual-compiled, needed by both `run_tests.exe` and `kungfu_server`, per the earlier dual-compilation reasoning — Makefile `INCLUDE_DIRS` and `server/CMakeLists.txt` both updated). The `char` color field needs an explicit `std::string(1, ...)` conversion or `nlohmann::json` serializes it as an integer, not `"w"`/`"b"` — a real gotcha, not hypothetical, caught by a dedicated test. | A2 (shares the vendoring pattern, no functional dependency) | 6 doctest cases: board/cellStates round-trip exactly, both scores, move history shape (`atMs`/`color`/`notation` per entry), `gameOver=false` mid-game, `gameOver=true` with `result` set, and an exact-field-set check (fails if a field is ever silently added or dropped). One iteration needed: `nlohmann::json`'s `{{...}}` initializer-list constructor is ambiguous between "array of 2-element arrays" and "object" and picked object for the board-comparison test — fixed by comparing against `json::parse()` on a string literal instead, not by changing the serializer. |
| **A5** ✅ | `WebSocketServer` (`include/server/WebSocketServer.hpp` + `src/server/WebSocketServer.cpp`, the one `src/server/` file that touches `websocketpp`/Asio directly, now excluded from the Makefile via `SERVER_ONLY_SRC`) + `ConnectionRegistry<Handle>` (`include/server/ConnectionRegistry.hpp`, header-only, templated so tests use plain `int`s instead of `websocketpp::connection_hdl`) wire A2→A3→A4 together for real: exactly one hardcoded `GameSession`, up to 2 connections (3rd closed immediately after its handshake completes - websocketpp's `open` handler fires post-handshake, so rejection is "accept then immediately close," not an HTTP-level refusal). Inbound text → A2 parse → A3 execute → A4 serialize → broadcast to every connection. `server/main.cpp` rewritten to actually run `WebSocketServer` (A1's echo handler and standalone linkage-proof are both superseded). | A1, A3, A4 | 4 doctest cases for `ConnectionRegistry` (accept-to-capacity, reject-beyond-capacity, insertion order, zero-capacity). Everything else manual — see verification steps below, and the two real bugs manual testing caught. |

**Two real bugs found and fixed during A5's own manual verification** (not
hypothetical - both reproduced, root-caused, and fixed before this task
was considered done):

1. **Server crash on send to a dead connection.** `broadcastState()`
   iterates every registered connection and calls `server_.send()`;
   `ConnectionRegistry` has no `remove()` yet (deliberately - real
   disconnect handling is Task D4's job). When a client process was
   killed mid-test, the next tick's `send()` to that now-dead handle
   threw `websocketpp::exception`, uncaught, which called
   `std::terminate()` and took down the *entire server process* over one
   dead connection. Fixed with a `try`/`catch` around the per-connection
   `send()` in `broadcastState()` - it doesn't add cleanup logic (still
   D4's job), it just stops one dead handle from being able to crash
   everything else. This is a baseline stability requirement, not
   something worth deferring to D4 just because D4 owns the *full*
   disconnect-handling feature.
2. **Simulated game clock ran at roughly half real-time speed.** The tick
   handler called `engine().wait(kTickMs)` using the *nominal* 16ms
   constant every time, but the real timer period measured during
   testing was closer to ~30ms (Windows timer granularity + asio/
   websocketpp overhead) - so a "2000ms" pawn move was actually taking
   ~3.8 real seconds to resolve, which looked exactly like a hung/broken
   move until root-caused. Fixed by measuring real elapsed time between
   ticks (`std::chrono::steady_clock`) and feeding *that* into
   `engine().wait()` instead of the hardcoded constant - the same
   dt-from-real-elapsed-time pattern the graphics `main.cpp`'s render
   loop already uses, not a new invention. `kTickMs` now only controls
   how often the timer fires, not how much simulated time passes per
   fire.

### Manual verification steps (A5)

Reproduces exactly what automated testing confirmed above. Needs three
terminals plus the build tools already set up for this repo.

```sh
# 1. Build the server (MSYS2/ucrt64 toolchain - adjust paths/PATH if your
#    cmake/ninja/g++ aren't already on PATH)
cmake -S server -B server/build -G Ninja -DCMAKE_CXX_COMPILER=g++
cmake --build server/build

# 2. Start it (terminal 1) - leave this running
./server/build/kungfu_server.exe
```

```sh
# 3. Terminal 2 - client A
python scripts/ws_test_client.py
```
```sh
# 4. Terminal 3 - client B
python scripts/ws_test_client.py
```

`ws_test_client.py` queues incoming broadcasts rather than printing them
the instant they arrive - printing from the background listener thread
while `input()` has a line half-typed was found to corrupt the pending
input buffer on Windows consoles (a typed `WPe2e4` could reach the server
as `WPe2e4WPe2e4`). Queued broadcasts get flushed to the terminal right
before each `input()` call, i.e. every time you press Enter (an empty
Enter works too, if you just want to flush without sending a command) -
so **each client terminal shows a batch of `< `-prefixed state broadcasts
right after every Enter you press**, not a continuous stream between
keystrokes. Press Enter once on an empty prompt in both terminals to
confirm both connections are live and receiving state before typing a
real command.

In **client A's** terminal, type a move and press Enter:
```
WPe2e4
```
Wait a couple of seconds (2000ms simulated travel time), then press Enter
on an empty prompt in **both** terminals to flush the queued broadcasts,
and watch the `"board"` field: rank-2 file-e (`board[6][4]`) goes from
`"wP"` to `"."`, rank-4 file-e (`board[4][4]`) goes from `"."` to `"wP"`.
Both clients should show the same resolved position - that's the "both
clients see the result" requirement.

```sh
# 5. Terminal 4 (or reuse 2/3 after Ctrl-C) - the 3rd connection
python scripts/ws_test_client.py
```
Expect: `Connected to ws://127.0.0.1:9002/` (the WS handshake itself
always succeeds - rejection is a close *right after*, not an HTTP-level
refusal), immediately followed by `[server closed the connection]`. If
you instead see it sit connected and receiving broadcasts, that's a
regression - it should never receive a single state message.

```sh
# 6. Confirm run_tests.exe still builds clean with SERVER_ONLY_SRC excluding
#    WebSocketServer.cpp (i.e. this build must NOT need websocketpp/Asio at all)
make
./run_tests.exe
```
Expect a clean build with no `websocketpp`/`asio` include errors, and
all test cases passing (111 as of this task, more once later tasks add
their own). If the build ever tries to compile `websocketpp` headers
here, `SERVER_ONLY_SRC` in the `Makefile` has regressed.

**Run step 6 from an MSYS2 terminal, not a plain PowerShell prompt.**
`make` isn't on PowerShell's `PATH` by default (it's an MSYS2/ucrt64
toolchain binary), so invoking it directly from PowerShell fails with
"make is not recognized," which looks like a broken build rather than a
missing shell. Open an MSYS2/ucrt64 shell (or otherwise ensure that
toolchain's `bin/` is on `PATH`) before running `make`/`./run_tests.exe`.

**Known limitation to expect, not a bug:** if you kill a client terminal
(Ctrl-C) instead of letting the script exit cleanly, the server keeps
that dead connection's slot occupied - `ConnectionRegistry` has no
`remove()` yet (Task D4). Restart `kungfu_server.exe` between test runs
if connections seem stuck at capacity.
| **A6** ✅ | `Logger` (`include/logging/Logger.hpp` + `src/logging/Logger.cpp`) — a plain instantiable class, **not a singleton**: `server/` and the future shell client (Phase F) are separate OS processes, so there's nothing to share via in-memory global state anyway. Deliberately generic (`log(message)` writes a timestamped line to every given `std::ostream*` sink, nulls skipped) - event-specific formatting stays in the caller, since the shell client will log entirely different kinds of lines that have nothing to do with `EventBus` events. `GameSession` gained an optional `Logger*` (`attachLogger()`), subscribed unconditionally in its constructor but gated on non-null so the **existing** default-constructed `GameSession()` used throughout every earlier test stays silent - no test breakage, no second constructor overload. `WebSocketServer` owns the real `Logger` (console + `server.log`, gitignored) and calls `session_.attachLogger(logger_)` once at construction; multiple sessions will share one `Logger` once `SessionManager` exists (Task D1), not one per session. | A5 | 5 doctest cases for `Logger` itself (multi-sink, timestamp prefix, multi-line ordering, empty-sinks no-op, null-sink-in-list skipped) + 5 for `GameSession`'s subscriptions (silent without `attachLogger`, move logs color/notation, jump logs a sound event, a resolved capture logs a score update, a king capture logs `lifecycle phase=end` with the result). Manual: fresh server run showed `server.log` picking up real `lifecycle phase=start` / `sound name=move` / `move color=w notation=e2e4` lines from an actual client command, confirming the `EventBus` wiring end-to-end, not just the unit tests. |

### Phase B — Home screen basic (deck item 2) ✅ complete (B1–B4)

| Task | What | Depends on | Tests |
|---|---|---|---|
| **B1** ✅ | `client/cli/main.cpp` skeleton: connects to the server via WS, no protocol yet — just a connectivity smoke test (mirrors A1's approach, but this time it's a real deliverable program, not throwaway). Built as a second executable target (`kungfu_client`) in `server/CMakeLists.txt` rather than its own top-level CMake project — unlike server/ vs `kungfu-graphics/cpp/`, there's no MSVC-vs-MinGW toolchain conflict to justify the split, so it reuses the already-fetched `asio_SOURCE_DIR`/`websocketpp_SOURCE_DIR` instead of a second `FetchContent` of both. Uses `websocketpp::client<config::asio_client>` with open/message/close/fail handlers; `client.run()` blocks on the io_context the same way `WebSocketServer::run()` does, so a live connection stays up and visibly prints every incoming broadcast rather than connecting and immediately exiting. | A5 | Manual: built via `cmake --build server/build`, ran `kungfu_client.exe` against a live `kungfu_server.exe` — printed `connected to server` then a stream of `received: {...}` tick broadcasts showing the standard starting position; server log showed the matching `[connect] ... / 101` + `connection accepted (1/2)` lines. `run_tests.exe` unaffected (121/121 — this task touched no dual-compiled sources). |
| **B2** ✅ | Server-side "join" message: `{ "type": "join", "username": "..." }` → assign White (1st joiner) / Black (2nd) / reject a 3rd (already enforced structurally by A5's 2-connection cap, now needs the username attached to the assignment and broadcast to both: "you are White", "opponent connected: <name>"). Pure assignment decision (`GameSession::handleJoin`, returns `JoinResult{ok, color, hasOpponent, error}`) lives in `GameSession` itself, same as A3's `handleCommand` - no separate class needed for two booleans. Wire format (not specified by the deck beyond the two example phrases) is this task's own design decision, all JSON to stay consistent with A4's state broadcasts rather than mixing plain strings and JSON on one connection: `{"type":"joined","color":"white"|"black","username":"..."}` to the joiner, `{"type":"opponent_joined","username":"..."}` to the already-connected opponent (only sent when `hasOpponent`), `{"type":"join_rejected","error":"ERROR ..."}` on rejection (session full, or a malformed join missing/empty `username` - a system-boundary validation, not requested explicitly but the same defensive posture as A2's error taxonomy). `WebSocketServer::onMessage` detects a join message by attempting `nlohmann::json::parse` first; a parse failure (e.g. a plain `"WQe2e5"` command, not valid JSON at all) or a parsed object whose `"type"` isn't `"join"` falls through to the existing `GameCommandParser` path unchanged. Connection→color mapping (`hdlToColor_`, keyed with `std::owner_less` per websocketpp's documented pattern for using `connection_hdl` as a map key) is populated by *join message* order, not connection-accept order. | A5, A4 (JSON) | 3 doctest cases for `GameSession::handleJoin` (1st join → White/no opponent, 2nd join → Black/opponent present, 3rd join → `ERROR SESSION_FULL`) - pure, decoupled from sockets, same extraction pattern as A5. Manual (networking glue, same convention as A5): a scripted 2-client + malformed-join + 3rd-connection-reject run against a live `kungfu_server.exe` (ad hoc verification script, not committed - reuses `scripts/ws_test_client.py`'s handshake/frame helpers) confirmed Alice→white, Bob→black, Alice received `opponent_joined` for Bob, a join missing `username` got `ERROR MALFORMED_JOIN`, and a 3rd connection was still closed post-handshake exactly as A5 established. `run_tests.exe` 124/124 (was 121, +3 new). |
| **B3** ✅ | Wire `client/cli`: prompt for username, send join, print assigned color + a "waiting for opponent" state until the second join arrives. `promptUsername()` loops until a non-empty, trimmed line is entered (a cheap system-boundary check - no point sending a join we already know the server will bounce as `ERROR MALFORMED_JOIN`). On the open handler, sends `{"type":"join","username":...}` immediately (B2's wire format). `handleServerMessage()` dispatches on `"type"`: `"joined"` prints "You are White/Black" plus "Waiting for opponent..." (White) or "Opponent already connected. Game starting!" (Black - true by construction, since Black is only ever assigned once White already joined); `"opponent_joined"` prints "Opponent connected: \<name\>. Game starting!" (only ever received by White, since B2 only sends it to the color that isn't the new joiner's); `"join_rejected"` prints the error. Anything else (the periodic state-tick broadcast, which has no `"type"` field at all) is silently ignored - board printing from it is Task B4's job, not this one. `kungfu_client`'s CMake target gained the `third_party/nlohmann` include dir it now needs. | B1, B2 | Manual: real two-client run (two live `kungfu_client.exe` processes, not the Python test client) against a live `kungfu_server.exe` - Alice joined first, saw "You are White."/"Waiting for opponent...", then Bob joined and Alice's *same still-running process* printed "Opponent connected: Bob. Game starting!"; Bob saw "You are Black."/"Opponent already connected." immediately. A 3rd real `kungfu_client.exe` instance handshook then printed "connection closed", matching A5's existing 2-connection cap. `run_tests.exe` unaffected (124/124 - no dual-compiled sources touched). |
| **B4** ✅ | Wire `client/cli`'s gameplay loop: on each state broadcast, print the board via `BoardPrinter`; read a line from stdin, forward it verbatim as the command string to the server (client doesn't need its own parser — A2 already lives server-side and is the source of truth). **Threading model, a real design decision this task had to make:** `websocketpp::client::run()` blocks on the asio event loop, which can't share a thread with a blocking `std::getline(std::cin, ...)` loop - so `client.run()` moves to its own thread, and the interactive stdin loop stays on `main()`. This is the same division of labor `scripts/ws_test_client.py` already uses (main thread owns `input()`, a background thread owns the socket) for the same reason that script's own git history exists for: a background thread printing straight to a Windows console while another thread has a line half-typed at a blocking read can corrupt the pending input buffer, not just interleave output. So the network thread never calls `std::cout` directly - `handleServerMessage()` only pushes text onto a mutex-guarded queue (`enqueueOutput`), and the main loop drains and prints that queue immediately before every blocking `std::getline()` (`drainOutput()`), mirroring `ws_test_client.py`'s exact fix. `kungfu_client`'s CMake target gained `src/text_io/BoardPrinter.cpp` + `include/text_io` (compiled standalone for this target only - it's pure/no-engine-dependency, and `kungfu_server` has no reason to link it, it never prints a board). | B3, A5 | Manual: a real scripted move/jump/capture sequence between two live `kungfu_client.exe` processes (Alice=White, Bob=Black) against a live `kungfu_server.exe` - `WPe2e4`, `BPd7d5`, `WPe4d5` (a real capture), `JBNb8` (a jump), each given real wall-clock time to resolve before the next dependent command. Final board printed by *both* clients agreed exactly: e2/e4/d7 empty, d5 holds `wP` (the capture), everything else unchanged; the jumped knight stayed at b8 (matches A3's own jump-doesn't-relocate-the-piece behavior). Server-side log corroborated with `move color=w notation=e2e4`, `move color=b notation=d7d5`, `move color=w notation=e4d5` + `score color=w newScore=1 delta=1` + `sound name=capture`, and `sound name=jump` / `move color=b notation=b8`. `run_tests.exe` unaffected (124/124 - no dual-compiled sources touched). |

### Phase C — Home screen upgraded (deck item 3)

| Task | What | Depends on | Tests |
|---|---|---|---|
| **C1** ✅ | `persistence/Database` (`include/persistence/Database.hpp` + `src/persistence/Database.cpp`) — thin wrapper over the SQLite C API: `exec()` for parameterless statements (DDL like `CREATE TABLE`), `prepare()`/`Statement` (move-only, RAII-finalized) for anything with a bound value - untrusted data (a username, a password hash) must go through bound parameters, never string-concatenated into `exec()`. Vendored `sqlite3.c`/`.h` (version 3.53.3, official amalgamation from sqlite.org, SHA3-256-verified against the published checksum before extracting) under `third_party/sqlite/` (committed, like `miniaudio/`), per the already-confirmed plan. **Real build-system finding, not anticipated by the original plan text:** `sqlite3.c` is genuine C, not C++ - it relies pervasively on implicit `void*`-to-`T*` conversions (`sqlite3DbMallocRaw()` etc.), legal in C but a hard error under C++'s stricter conversion rules. Compiling it via `g++` (as every other `SOURCES` entry is) failed with dozens of "invalid conversion from 'void\*'" errors, confirmed by a real build attempt. Fixed two ways: **(1)** the Makefile now compiles `third_party/sqlite/sqlite3.o` via a dedicated `gcc`/`-std=c11` rule and links that object into the final `g++` link step, instead of listing `sqlite3.c` in `SOURCES` directly; **(2)** `server/CMakeLists.txt`'s `project(...)` call gained `C` alongside `CXX` - with only `CXX` enabled, CMake had no C compiler configured and silently *dropped* `sqlite3.c` from the build entirely (no error, just missing from both the ninja steps and the final link line) rather than failing loudly, which was the more confusing half of this to track down. Both fixes are the standard, sqlite.org-documented way to embed the amalgamation in a C++ project, not repo-specific workarounds. `third_party/README.md` updated (also backfilled a missing `nlohmann/` entry in the "vendored and committed" list while touching that file). | — | 6 doctest cases against `:memory:` databases: create table + insert + query round-trip, a query with no matching rows, multi-row iteration order, a null column via `isNull()`, and `exec`/`prepare` both throwing `std::runtime_error` on malformed SQL. `run_tests.exe` 130/130 (was 124, +6 new). Manual: `kungfu_server.exe` still builds and starts/listens cleanly via CMake/Ninja with `persistence`/`sqlite3.c` now linked in (not yet wired into any request path - that's `UserRepository`/C2's job). |
| **C2** ✅ | `persistence/UserRepository` (`include/persistence/UserRepository.hpp` + `src/persistence/UserRepository.cpp`) — `ensureSchema()` (idempotent `CREATE TABLE IF NOT EXISTS`), `createUser()`, `findByUsername()` (`std::optional<User>`), `verifyPassword()`, `updateRating()`. Vendored Brad Conte's public-domain SHA-256 (`third_party/sha256/`, from `B-Con/crypto-algorithms` commit `cfbde48414baacf51fc7c74f275190881f037d32`), per the resolved open question. Salt-generation (`std::mt19937_64` seeded from `std::random_device`, 16 random bytes hex-encoded) and hash-and-compare (`SHA-256(salt + password)`, hex digest) are private helpers in `UserRepository.cpp`, not a separate class - nothing else in the codebase needs them yet. `verifyPassword()` returns `false` for both "no such user" and "wrong password" - deliberately not distinguished, so a login attempt can't be used to enumerate registered usernames. `createUser()` doesn't pre-check for a duplicate username itself - it just tries the `INSERT` and lets the table's own `UNIQUE` constraint surface as a `std::runtime_error` via `Database`'s existing exception path (Task C4's auto-register flow is expected to call `findByUsername()` first anyway). **Second real build-system finding, same shape as C1's:** unlike `sqlite3.c`, `sha256.c` is plain portable C with no implicit `void*`-conversion reliance, and compiles cleanly as C++ directly (confirmed with `-Wall -Wextra`, zero warnings) - so it's built as C++ in both the Makefile (just added straight to `SOURCES`, no separate `gcc` rule needed) and `server/CMakeLists.txt`. But CMake needed an explicit `set_source_files_properties(... PROPERTIES LANGUAGE CXX)` override for it - once C1 enabled the `C` language (for `sqlite3.c`), CMake's default per-file language dispatch would otherwise compile *any* `.c` source, including this one, with the C compiler, producing plain-C-linkage symbols that `sha256.h` (no `extern "C"` guards at all) doesn't declare a mismatch-safe way to call from C++. Confirmed the override actually took effect from the real ninja build log: `sha256.c.obj` compiles as a "CXX object", `sqlite3.c.obj` still compiles as a "C object", in the same build. | C1 | 8 doctest cases against `:memory:` databases: create+find+wrong-password-rejected (the plan's own three, combined into one scenario plus explicit checks), nonexistent-username lookup returns `nullopt`, nonexistent-username `verifyPassword` returns `false` without throwing, duplicate-username `createUser` throws, rating update persists, rating update on a nonexistent user returns `false`, two users with the same password get independently-verifiable (per-user-salted) hashes, and `ensureSchema()` is idempotent. `run_tests.exe` 138/138 (was 130, +8 new). Manual: `kungfu_server.exe` rebuilt clean via CMake/Ninja with `persistence`/`sha256.c` linked in and still starts/listens (not yet wired into any request path - that's `AuthService`/C3's job). |
| **C3** ✅ | `server/AuthService` (`include/server/AuthService.hpp` + `src/server/AuthService.cpp`) — `authenticate(username, password)`: never-seen-before username auto-registers (`UserRepository::createUser`) and accepts unconditionally, per the resolved C4 open question; existing username accepts only if `verifyPassword()` matches, else `ERROR AUTH_FAILED`. Wired into `GameSession::handleJoin`, now `handleJoin(username, password)` - authenticates *before* any color-slot assignment (a rejected login never consumes White/Black). `GameSession` gained an optional `AuthService*` (`attachAuthService()`, same optional-attach pattern A6 established for `Logger*`) - but unlike `Logger`'s silent-no-op-when-absent default, an unattached `AuthService` makes `handleJoin` fail **closed** (`ERROR AUTH_NOT_CONFIGURED`), not open - "no auth configured" must never silently mean "let everyone in". The wire join message (B2) now requires both `"username"` and `"password"`; either missing is `ERROR MALFORMED_JOIN` (unchanged error, extended condition). `WebSocketServer` now owns the real `Database`/`UserRepository`/`AuthService` for the process (`data/kungfu_chess.db`, per the plan's already-approved schema/location - new gitignored `data/`), constructed in declaration order (`database_` before `userRepository_` before `authService_` before `session_`) and calls `userRepository_.ensureSchema()` once at startup; `server/main.cpp` creates `data/` via `std::filesystem::create_directories` first, since SQLite can create the `.db` file itself but not a missing parent directory. **`client/cli` is *not* updated to send a password yet** - it still only sends `{"type":"join","username":...}` (B3), so every join from today's shell client now gets `ERROR MALFORMED_JOIN` until Task C4 wires the password prompt through; this is expected, not a regression, and matches C4 being the very next task. | C2, B2 | 5 new doctest cases for `AuthService` itself (auto-register + accept, existing-user correct password accepted without re-registering, existing-user wrong password rejected, auto-register locks in the first password so a later different password fails, two usernames auto-register independently and aren't cross-acceptable) plus 3 new/updated `GameSession` cases (no `AuthService` attached fails closed, wrong password on an existing username is rejected, a rejected join doesn't consume a color slot - confirmed by a subsequent real join still landing on the correct next slot) - the existing 3 B2 join tests updated to attach a real `AuthService` and pass a password. `run_tests.exe` 146/146 (was 138, +8 new). Manual: a real WS-protocol script (not `client/cli`, which doesn't send passwords yet - see above) against a live `kungfu_server.exe` confirmed all four cases end-to-end over the wire in one session: missing password → `MALFORMED_JOIN`; never-seen-before username + password → auto-registered and accepted as White; same username + wrong password on a second connection → `AUTH_FAILED`, confirmed *not* to consume that connection's color slot; same username + correct password on that same second connection → accepted as Black. Server-side log corroborated (`connection accepted (1/2)` / `(2/2)`, clean disconnects, no crashes). Confirmed `data/kungfu_chess.db` is actually created on startup via a real (non-`:memory:`) run. |
| **C4** ✅ | Wire `client/cli`'s login prompt to ask for username **and** password; handle the reject/accept responses. Restructured the connect flow: the join is no longer auto-sent from the open handler - `main()` now waits (condition-variable, not polling) for the connection to open, then runs a **login retry loop** that prompts username + password, sends the join, waits for the server's accept/reject, and on rejection prints `"Login failed (<error>). Try again."` and re-prompts instead of leaving the user stuck typing into a session they never joined. A small `LoginState` (mutex + condition_variable + ok/error fields) coordinates the main thread's wait with the asynchronous response arriving on the network thread via `handleServerMessage()` - the network thread still never touches `std::cout` directly (see the file's threading-model comment), it only signals `LoginState` and enqueues text the same way it already did for everything else. `join_rejected` no longer also `enqueueOutput()`s a message itself - only the retry loop prints the failure now, avoiding a duplicate/racing message against the loop's own immediately-following prompt. | C3, B3 | Manual (this task is inherently a socket/console-interaction change, not pure logic - no doctest coverage, matching every other `client/cli` task's convention): built via `cmake --build server/build --target kungfu_client`. Two real `kungfu_client.exe` runs against a live `kungfu_server.exe` - (1) a brand-new username + password auto-registers and is accepted as White in one shot; (2) the same username with the *wrong* password on a second connection gets `"Login failed (ERROR AUTH_FAILED). Try again."` and re-prompts, then the same username + *correct* password on the second attempt is accepted (Black, since White was already taken by run 1). Server-side log corroborated exactly 2 connections accepted (1/2, then 2/2). `run_tests.exe` unaffected (146/146 - `client/cli` is never dual-compiled). |
| **C5** | ELO rating update on game end: subscribe to `onGameLifecycle("end", ...)` (same EventBus hook A6 already established the pattern for), compute both players' new ratings via standard ELO, persist via `UserRepository`, include updated ratings in the next state broadcast. **Open question below:** K-factor value (deck doesn't specify). | C2, A6 | doctest: known before-ratings + known result → known after-ratings, check against a hand-computed ELO example. |

### Phase D — Matchmaking (deck item 4)

| Task | What | Depends on | Tests |
|---|---|---|---|
| **D1** ✅ | `server/SessionManager` (`include/server/SessionManager.hpp`, header-only like `ConnectionRegistry.hpp`) — generalizes A5's hardcoded single session into N concurrent sessions, connection→session routing. Templated on the connection-id type **and its comparator** (`Compare = std::less<ConnectionId>` by default, so plain-`int` tests just work; `WebSocketServer` instantiates it with `websocketpp::connection_hdl` + `std::owner_less<connection_hdl>` - the same comparator `hdlToColor_` already needed, since `connection_hdl` is a `std::weak_ptr<void>` with no `operator<`/`operator==` of its own). Internally composes one `ConnectionRegistry<ConnectionId>` per session (reused as-is, not reimplemented) rather than a single global one. **Real design decision, confirmed before starting:** `createSession()` is a separate, explicit call - `tryAdd()` never auto-creates a session when every existing one is full. Considered auto-creating on overflow (would've let a 3rd/4th connection spill into a brand-new session) and deliberately rejected it: spectator support (not yet designed as of this task) should be the thing deciding what an overflow connection does - joining/watching an *existing* session, not spinning up an unrelated new one; auto-creating now would lock in the wrong shape and have to be torn out later. So **this task changes no observable behavior at all** - `WebSocketServer`'s constructor calls `createSession()` exactly once (mirroring A5's single hardcoded session precisely), and a 3rd connection is still rejected outright, exactly like A5. Task D2's matchmaking is what will call `createSession()` for real once players are matched. `WebSocketServer` refactored accordingly: `GameSession session_` → `std::vector<std::unique_ptr<GameSession>> sessions_` parallel-indexed by `SessionManager`'s session ids (`unique_ptr` since `GameSession` is neither copyable nor needs to be movable, and this keeps references stable as the vector grows); `broadcastState()`/the tick loop/`tryHandleJoin()` all gained a `sessionId` parameter and now route through `sessions_[sessionId]`/`sessionManager_.connectionsIn(sessionId)` instead of one global `session_`/`registry_`. `Database`/`UserRepository`/`AuthService` stay process-wide (not per-session) - only game state multiplies. | A5 | 9 new doctest cases for `SessionManager<int>` (sequential session ids, per-session capacity enforcement, rejection beyond capacity, `tryAdd` never auto-creating an unknown session id, negative id rejected, routing lookup both hit and miss, connection order preserved, two independent sessions not sharing capacity or connection lists). `run_tests.exe` 155/155 (was 146, +9 new - `WebSocketServer.cpp` itself stays excluded from the Makefile build, unaffected either way). Manual: rebuilt `kungfu_server.exe` via CMake/Ninja; re-ran the exact C3 four-case auth script (missing password / auto-register / wrong password / correct password) - all four still pass verbatim through the new per-session routing; confirmed a real 3rd connection still gets a proper WebSocket close frame (`opcode 0x8`, status 1013, reason `"session full"`) after the session's 2 slots were taken, matching A5 exactly; played a real `e2e4`/`d7d5` move sequence between two live `kungfu_client.exe` processes and confirmed both saw the identical resolved board through the new per-session tick/broadcast loop. Server log corroborated every step (`connection accepted to session 0 (1/2)` / `(2/2)`, `rejecting connection - all sessions full`, move/sound events). |
| **D2** ✅ | `server/MatchmakingQueue` (`include/server/MatchmakingQueue.hpp`, header-only like `ConnectionRegistry.hpp`/`SessionManager.hpp`) — pure "who plays whom" pairing decision, decoupled from sockets/`GameSession`/`SessionManager` entirely. Templated on the seeker-id type so tests use plain `int`s. `enqueue(id, rating)` and `tryMatch()` are deliberately separate calls, not one enqueue-that-auto-matches - the plan's own wording ("on each new enqueue (or on a periodic scan), check the whole queue") frames the trigger as the *caller's* decision, not this class's job. `tryMatch()` scans the **whole** queue (not just adjacent entries - a doctest specifically seeds an out-of-range seeker *between* two in-range ones to prove this), removes and returns the first pair within ±100 rating (inclusive at exactly 100, confirmed by a boundary doctest at 100 vs. 101), or `std::nullopt` with the queue left untouched if none qualify yet. Also has `remove()`/`contains()` for Task D3 (60s timeout)/D4 (disconnect-while-queued) to use later. **Scope decision, confirmed before starting:** this task is the pure class only, **not wired into `WebSocketServer`** - matches the pattern `Database` (C1) was built standalone and wired in later (C3). Full `"Play"` wiring (new wire message, `WebSocketServer`'s connection lifecycle changing so login no longer auto-assigns a color, `client/cli` gaining a post-login "waiting for match" state) lands together with Task D3's 60s timeout, since D3 needs the real end-to-end flow to test expiry against anyway - building the wiring twice would be wasted work. Identity comparisons (`remove()`/`contains()`) use plain `operator==`, fine for the int ids these tests use; `websocketpp::connection_hdl` has no `operator==` (same root cause `SessionManager`'s `Compare` template parameter solved for ordering), so whatever wires this in will need either a small locally-allocated seeker id instead of the raw hdl, or an owner-equality-style comparator - deliberately not solved here since it isn't needed until D3 actually wires this in. | D1, C2 (needs ratings) | 11 new doctest cases: in-range pair matched, out-of-range pair not matched, exactly-100 matched (inclusive boundary), 101 not matched (just past it), rating-diff symmetry regardless of enqueue order, a non-adjacent in-range pair found across an out-of-range seeker sitting between them (the plan's own explicit requirement), empty queue and single-seeker queue both return `nullopt`, `remove()` takes a seeker out without matching them, `remove()` on an absent seeker returns `false`, `contains()` correctness. `run_tests.exe` 166/166 (was 155, +11 new). No manual verification - nothing wired to a real socket yet, consistent with the scope decision above; `cmake --build` confirmed as a no-op (`ninja: no work to do`), corroborating that this task really did touch nothing `WebSocketServer` depends on. |
| **D3** ✅ | 1-minute timeout: a `steady_timer` per seeker; if unmatched after 60s, dequeue and send "can't find opponent". **This is also where the full "Play" wiring deferred from D2 landed** (per the scope decision recorded in D2's own row): real matchmaking end-to-end, replacing A5/B2's "connect and immediately join the one pre-existing session" lifecycle entirely. New connection lifecycle: `onOpen()` accepts unconditionally (no session exists yet, nothing to cap); `{"type":"join",...}` is now login-only via `AuthService` directly (`GameSession` has no `AuthService` dependency any more - see below) → `{"type":"logged_in","username":...}`; `{"type":"play"}` enqueues into `MatchmakingQueue<int>` (Task D2) with the account's current rating and starts a per-seeker `steady_timer`, responding `{"type":"searching"}` if not immediately matched; on a match (checked once per enqueue - single-threaded `io_context` means one `tryMatch()` call per enqueue is provably sufficient, not just convenient, since a latent matchable pair could only ever involve the seeker that just enqueued) a session is created via `createSession()`, both connections added, each gets `{"type":"joined","color":...,"username":...,"opponent":...}` - matching is atomic now (both players known at the same instant), so B2's old async `opponent_joined` notification and the `hdlToColor_` map it needed for "who's the other connection in this session" are both gone entirely; on a 60s timeout, `{"type":"matchmaking_timeout","error":"ERROR NO_OPPONENT_FOUND"}`. **`GameSession` API change:** `handleJoin(username, password)` (C3) is gone - authentication moves entirely to `WebSocketServer` (there's no session to call it on at login time any more); `GameSession` gained a simpler `assignSeat(username)` with no password/auth involved, and lost its `AuthService*`/`attachAuthService()` entirely. This simplified `test_game_session.cpp`'s C3-era join tests back down to plain username-based checks (no `Database`/`UserRepository`/`AuthService` fixture needed any more - `AuthService`'s own accept/reject decision stays tested independently, unaffected, in `test_auth_service.cpp`). **Connection identity for the queue:** `websocketpp::connection_hdl` has no `operator==` (the gap D2's own row explicitly deferred) - solved with a small server-side incrementing `int seekerId`, mapped back to the real `hdl` via `queuedSeekers_`, rather than trying to make `MatchmakingQueue` itself hdl-aware. **Real timer, not assumed elapsed time:** the per-seeker timer's fire callback measures actual wall-clock elapsed time via `std::chrono::steady_clock` (not assumed-exactly-60000ms) before consulting `MatchmakingTimeout::shouldTimeOut()` - same "don't trust the nominal timer value" lesson A5's own tick-timing bug already established, now demonstrably meaningful here too (confirmed timestamps below). `client/cli` updated to match: after `"logged_in"`, sends `{"type":"play"}` and prints "Searching for an opponent..." itself (synchronously, main thread) rather than waiting on the server's own `"searching"` ack - the match wait can block up to 60 real seconds and nothing drains the output queue while blocked on it, so waiting for the ack would leave the user staring at nothing for up to a minute; then waits for `"joined"` (prints color + opponent, proceeds to the existing gameplay loop) or `"matchmaking_timeout"` (prints the error, exits - no auto-retry). | D2 | 3 new doctest cases for `MatchmakingTimeout::shouldTimeOut()` (past the deadline while still queued → true; before the deadline → false; past the deadline but no longer queued, i.e. matched in the interim → false) - `run_tests.exe` 166/166 (net unchanged: 6 obsolete C3-era auth-based `GameSession` join tests removed, 3 new plain `assignSeat` tests + 3 new timeout-decision tests added). Manual: rebuilt `kungfu_server.exe`/`kungfu_client.exe` via CMake/Ninja; two live clients (Alice, Bob) both auto-registered, both sent `{"type":"play"}`, were matched immediately (both default-rated 1200, well within ±100) with `"You are Black. Playing against Bob."`/`"You are White. Playing against Alice."`, played a real `e2e4`/`d7d5` exchange and both resolved the identical final board; server log confirmed `"connection accepted"` (no session yet) for both, then `"lifecycle phase=start"` (a session created) only *after* both connected and matched, exactly matching the new design. **Real 60-second timeout also verified for real** - a single client (`Solo`) logged in, sent `"play"`, and was left alone in the queue; wall-clock timestamps before/after the run measured exactly 60 seconds elapsed before the client received `"Matchmaking failed (ERROR NO_OPPONENT_FOUND)."` and exited cleanly; server log showed the connection accepted at `15:04:40` and disconnected at `15:05:40`, confirming the real timer (not a doctest stand-in) actually fired at the real 60-second mark. |
| **D4** ✅ | Disconnect-mid-game handling, **with reconnect support** (open question below resolved: yes). `WebSocketServer` gains `onClose()`, registered as both `set_close_handler`/`set_fail_handler` (a connection that dies before completing the WS handshake fires `fail`, not `close` - both route to the same handler). A closed hdl still only queued in `matchmakingQueue_` (never matched) is just dequeued - reconnect is specifically for a game already in progress, the search queue already has its own independent 60s give-up timeout (D3), unaffected. A closed hdl that *was* in an unfinished session's seat: `GameSession::markDisconnected(color)` (new per-color `bool ...Connected_`, defaulted true, flipped by `markDisconnected()`/`markReconnected()`/read by `isConnected()`), `SessionManager::remove(hdl)` (new - frees the seat's connection-registry slot without tearing down the session itself; `ConnectionRegistry` also gained `remove()`, templated on the same `Compare` `SessionManager` already carries, since `connection_hdl`/`std::weak_ptr<void>` has no `operator==` for `std::find` to use), and a 20s `asio::steady_timer` starts (`startDisconnectTimer()`, same real-elapsed-time-via-`steady_clock` measuring pattern as D3's 60s matchmaking timeout, decision extracted to a pure `DisconnectTimeout::shouldAutoResign(elapsedMs, stillDisconnected)` mirroring `MatchmakingTimeout::shouldTimeOut()`). **Countdown broadcast:** no separate per-second broadcast loop - every existing tick's `broadcastState()` call now also computes each disconnected seat's remaining ms fresh from `steady_clock` and passes it to a new `GameStateSerializer::DisconnectStatus{whiteRemainingMs, blackRemainingMs}` overload of `serialize()`, emitted as `whiteDisconnectMs`/`blackDisconnectMs` (JSON `null` when that seat isn't disconnected). Deliberately *not* added to `GameSnapshot`/`GameEngine` itself - which color's connection is down is connection-layer state, not game state, so it doesn't belong on the read-model the connection-less local graphics renderer also consumes. **Reconnect:** a new `{"type":"join",...}` re-authenticating as a username that `WebSocketServer`'s new `usernameToSessionId_` map (populated in `handleMatch()`, alongside `sessions_`/`sessionManager_`) still associates with a session, whose seat `colorOf(username)` resolves and `isConnected(color)` reports false, and whose `GameSnapshot::gameOver` is still false, is treated as a reconnect (`tryReconnect()`, called from `handleLogin()` right after `AuthService::authenticate()` succeeds) instead of a fresh login: the new hdl is `sessionManager_.tryAdd()`'d back into the session, `authenticatedUsers_[hdl]` is set, `GameSession::markReconnected(color)` flips the seat back, the pending 20s timer is found (keyed by `(sessionId, color)` in a new `disconnectedSeats_` map) and `.cancel()`'d, and the client gets `{"type":"reconnected","color":...,"username":...,"opponent":...}` (opponent name via new `GameSession::usernameFor(char)`) plus an immediate `broadcastState()`. **Auto-resign:** if the 20s timer instead fires with the seat still disconnected, `handleDisconnectTimeout()` calls a new `GameEngine::resign(char color)` - deliberately the *exact same* `gameOver`/`winnerColor_`/`onGameLifecycle("end", ...)` path a king capture already drives (`applyCaptureEvents()`), so `GameSnapshot::gameOver` stays the one source of truth regardless of *why* the game ended, and any future `onGameLifecycle`-driven feature (e.g. ELO updates, C5) picks up an auto-resign automatically with no separate code path. `authenticatedUsers_[hdl]` is no longer erased anywhere mid-connection (it previously only lived through matchmaking) - `onClose()` needs the hdl→username binding to still be there after a match, and it's erased there instead, once the hdl is confirmed dead. | D1, A6 | 22 new doctest cases: `DisconnectTimeout::shouldAutoResign` (3, mirroring `MatchmakingTimeout`'s own 3), `ConnectionRegistry::remove()` (3: frees a slot, false on unknown handle, freed slot reusable up to capacity again), `SessionManager::remove()` (3, same shape), `GameEngine::resign()` (4: ends the game in favor of the other color both ways, fires the lifecycle event, no-ops if the game already ended), `GameSession` `colorOf()`/`usernameFor()`/connection-status (6), `GameStateSerializer::DisconnectStatus` (2, plus the existing "exactly the approved field set" test updated for the two new keys). `run_tests.exe` 186/186 (was ~164, +22 new - rebuilt via a manual g++ invocation mirroring the Makefile exactly, since `make` itself isn't installed on this machine, only the MSYS2 UCRT64 toolchain; `WebSocketServer.cpp` itself stays excluded from this build as before, needing real websocketpp/Asio). `kungfu_server`/`kungfu_client` (CMake/Ninja, which *does* compile `WebSocketServer.cpp`) rebuilt clean, zero warnings. **Live manual repro run for real** (own follow-up session, same bar as D1/D3): `kungfu_server.exe` + a raw-WS Python observer (reused `ws_test_client.py`'s handshake/frame primitives, since it can't be driven interactively as a background process) as one player, real `kungfu_client.exe` as the other. **One real bug found and fixed during this verification** (not hypothetical): `client/cli` had no handler for the new `"reconnected"` message type at all, so attempting to reconnect through the real client just hung forever waiting for a login response that would never arrive - fixed by adding a `"reconnected"` branch (mirroring `"joined"`) that skips straight to the gameplay loop instead of re-entering matchmaking; also added `logger_.log()` calls around disconnect/reconnect/auto-resign, since none of the three had any `server.log` signal at all before this (separate follow-up commit). All three paths confirmed with real wall-clock timestamps, cross-checked across three independent sources (client stdout, `server.log`, and the observer's raw JSON) - **path 1** (live countdown): `blackDisconnectMs` broadcast every tick, decrementing smoothly and matching wall-clock exactly (20000→19969→18975→...→14992 over the first ~5s); **path 2** (reconnect): real `kungfu_client.exe` reconnected 5.2s after disconnecting, printed `"Reconnected as Black. Playing against D4Bob3."`, `server.log` shows `reconnect session=1 color=b username=D4Alice3 - countdown cancelled`, and the observer's very next tick shows `blackDisconnectMs` reset to `null`; **path 3** (auto-resign): two independent full-length runs both auto-resigned at *exactly* 20 real seconds after disconnect (`18:14:18`→`18:14:38` and `18:16:48`→`18:17:08`, `server.log`'s own timestamps, second-granularity), `gameOver` flips true with `result="White Wins"` (the disconnected side was Black both times) at the same moment `server.log` logs `auto-resign session=... - 20s disconnect window elapsed with no reconnect`; also incidentally confirmed a late reconnect attempt arriving *after* auto-resign already fired correctly falls through to a fresh login instead of resurrecting the finished game (the `gameOver` guard in `tryReconnect()`), observed when a mistimed manual attempt landed 10s after that session's own auto-resign. |

### Phase E — Rooms (deck item 5)

| Task | What | Depends on | Tests |
|---|---|---|---|
| **E1** ✅ | `server/RoomRegistry` (pure, header + `.cpp` for the real id generator) — create-room/join-by-ID discovery, room id ↔ session id only. **Architectural decision, confirmed before starting:** role assignment (1st joiner = White, 2nd = Black, 3rd+ = spectator) is *not* RoomRegistry's job or a second, room-only concept - it's `GameSession::join()` (renamed from `assignSeat()`, which used to reject a 3rd call with `ERROR SESSION_FULL` and now instead pushes onto a new `spectatorUsernames_` list and returns `color='s'`), so a matchmaking-created session and a room-created session both reach spectator support through the exact same method, not two implementations of the same rule. `colorOf()` deliberately keeps reporting `'\0'` for a spectator (same as "never joined") since its only consumer (D4's disconnect-countdown machinery) only needs "does this hold a resignable seat" - a new `isSpectator()` is the query for telling those two `'\0'` cases apart. Room id: 6-character code from a 32-char alphanumeric charset excluding visually-ambiguous characters (0/1/I/O), generated via an injectable `std::function<std::string()>` (defaults to a real `std::mt19937`-backed generator) so tests can supply a fixed sequence - including deliberately forcing a collision to exercise the retry loop - without depending on true randomness. **Scope decision, confirmed before starting (mirrors D2→D3):** this task is pure logic only, *not* wired into `WebSocketServer` - no `create_room`/`join_room` wire messages, no `ConnectionRegistry` capacity-cap change yet. That lands with E3, once `client/cli` has a real "Room" flow to wire and manually verify against, same reasoning D2 gave for deferring `MatchmakingQueue`'s wiring to D3. | D1 | 12 new doctest cases: `GameSession` (5: 3rd/4th join is a spectator not a rejection, `isSpectator` true only for actual spectators, multiple spectators tracked independently, `colorOf` still `'\0'` for a spectator) + `RoomRegistry` (7: `createRoom`/`sessionForRoom` round-trip, unknown id → `nullopt`, independent rooms don't collide, a forced generator collision is retried until unique, the real generator's format/ambiguous-character exclusion, the real registry produces distinct ids across 50 creates). `run_tests.exe` 198/198 (was 186, +12 new). `kungfu_server`/`kungfu_client` (CMake/Ninja) rebuilt clean against the renamed `GameSession::join()` API - `RoomRegistry.cpp` itself isn't in that build yet (CMake's source glob needs a reconfigure to pick up a brand-new file, and nothing references it until E3 wires it in anyway), consistent with the scope decision above. |
| **E2** | Spectator enforcement: `GameSession`/A3's command handler rejects move/jump commands from a spectator connection before they ever reach `GameEngine`. **Real complexity found while designing E1, flagged before E2 starts:** no sender-identity check exists yet, for anyone - `GameCommandParser::ParsedCommand.color` is a color *claimed by the wire command text* ("WQe2e5"), never checked against which connection actually sent it, so today Black's own connection could send a White-claiming command and it would be accepted. E2 needs some sender-identity plumbing to hang `isSpectator()` off of, and none exists even for the two real players yet - this is more foundational than a single `isSpectator()` check. | E1, A3 | doctest: spectator-submitted command is rejected without mutating engine state. |
| **E3** | Wire `client/cli`'s "Room" flow: Create/Join/Cancel prompt, room ID displayed at the top of the text UI. **This is also where E1's deferred wiring lands** (per E1's own scope decision): `create_room`/`join_room` wire messages, the `ConnectionRegistry` capacity-cap raise (currently `kMaxConnectionsPerSession = 2`, uniform across every session regardless of origin - raising it is what actually makes room spectating possible, and structurally leaves the door open for a future spectate-a-matchmaking-game entry point too, without requiring one now), and the room's `onGameLifecycle("start",...)` firing on the 2nd join rather than at room creation (parity with matchmaking's own "exactly 2 known participants" moment - `createSession()` currently starts the game unconditionally and immediately, which only happened to be safe because it was only ever called once matchmaking already had 2 players). **Open question, confirmed before E1 started:** room join requires the same username+password login as matchmaking - no anonymous spectating. | E1, B4 | Manual: three CLI clients — two players, one spectator — confirm spectator sees state but can't move. |

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

1. ~~**A2 — jump command syntax.**~~ **Resolved:** `J`-prefix (`"JWPe2"`).
   Documented as a design-decision comment in `include/server/GameCommandParser.hpp`.
2. ~~**A4 — does the reduced state payload include `captureFlashes`?**~~
   **Resolved: excluded.** Same render-loop-only category as
   `moveProgress`/`moveTargets`; revisit only if/when the graphics binary
   gets network support (separate, unscoped future task).
3. ~~**C2 — which specific SHA-256 implementation to vendor?**~~
   **Resolved:** Brad Conte's `sha256.h`/`.c` (public domain), as
   originally suggested. Vendor when C2 starts.
4. ~~**C4 — auto-register vs. explicit register/login.**~~ **Resolved:
   auto-register** — a never-seen-before username on the single login
   screen gets created automatically; the deck only describes one login
   screen, no separate register flow.
5. ~~**C5 — ELO K-factor.**~~ **Resolved: 32.**
6. ~~**D4 — is mid-game reconnect supported at all**~~ **Resolved: yes.**
   Re-logging in as the disconnected username within the 20s window
   resumes the same seat (`WebSocketServer::tryReconnect()`) - the C4
   username+password login already gives enough identity to match a
   reconnecting connection back to its `GameSession`/color, so supporting
   it was cheap relative to the UX cost of resigning over a brief network
   blip. See D4's row above for the full design.
7. ~~**E1 — room ID format/length.**~~ **Resolved: 6-character
   alphanumeric, excluding visually ambiguous characters** (0/1/I/O) -
   meant to be read off one player's screen and typed by another.
   Implemented in `RoomIdGenerator::generate()`.
8. ~~**Concurrency model**~~ **Resolved: confirmed** - single-threaded
   `io_context` with a chained `steady_timer`, no worker threads, as
   already implemented since A5. D1 builds on this model explicitly (every
   session ticks/broadcasts on this same one thread, no per-session
   timers or locking).
9. **Should there be a match-history table** beyond the live `rating`
   field, for a future profile/history screen? Not required by the deck
   items given, cheap to add now, more annoying to retrofit later.
