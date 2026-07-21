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

1. ~~**A2 — jump command syntax.**~~ **Resolved:** `J`-prefix (`"JWPe2"`).
   Documented as a design-decision comment in `include/server/GameCommandParser.hpp`.
2. ~~**A4 — does the reduced state payload include `captureFlashes`?**~~
   **Resolved: excluded.** Same render-loop-only category as
   `moveProgress`/`moveTargets`; revisit only if/when the graphics binary
   gets network support (separate, unscoped future task).
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
