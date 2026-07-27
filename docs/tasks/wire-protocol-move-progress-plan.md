# Kung Fu Chess — wire protocol: moveProgress/moveTargets for smooth Online Play animation (TODO / reference plan)

> **Status: task breakdown confirmed, I1 done.** `GameStateSerializer`
> now emits the sparse `activeMoves` array described in Decision 1 below.
> Pick up at **I2** (`GameStateDeserializer`) when resuming.

## How to use this file

Same convention as `server-phase-plan.md`/`graphics-networked-client-plan.md`:
work through tasks **in order** (a task's "Depends on" column means what it
says), one small commit per task, doctest-first where the task has pure
logic to test, real-socket/scripted verification where it doesn't (same
"real sockets, not real terminals" convention those two plans already
established for networking-glue tasks that can't be doctested). Confirm
`run_tests.exe` and the graphics build (`KungFuChess`, MSVC/CMake) still
build/pass clean before moving to the next task.

## Goal

`graphics-networked-client-plan.md`'s own Decision 1 (confirmed before H1
started) deliberately deferred this: `GameStateSerializer` excludes
`moveProgress`/`moveTargets` (per-frame slide interpolation) and
`captureFlashes`, so the networked graphics client (H2's translator)
defaults those fields and pieces **snap** between network ticks instead of
sliding, with no capture-flash effect — "revisit only if the degraded
fidelity turns out to matter once it's actually played." Online Play (H1–H8)
is done and the user has played it live; this plan is that revisit, scoped
to `moveProgress`/`moveTargets` only (real slide animation). `captureFlashes`
stays out of scope — a separate, unscoped future task, same as the original
decision left it.

## Decisions already confirmed (don't re-litigate these)

Resolved via the bandwidth/test/G4 analysis already reviewed before this
file was written:

1. **Sparse encoding, not a dense 64-cell grid.** `GameEngine::snapshot()`
   (`src/game_engine/GameEngine.cpp:179-201`) only ever writes a real
   `moveTargets`/`moveProgress` value into a cell when
   `arbiter.getMoveProgress()` returns true for it — the exact same
   condition that already sets `cellStates[r][c] = "move"`. Every other
   cell sits at the default `Position{0,0}`/`0.0`. A naive full-grid
   encoding would add ~2.2KB/tick/connection (64 cells × ~30 bytes each) at
   the existing 60Hz (`kTickMs = 16`, `include/server/WebSocketServer.hpp:205`)
   broadcast rate — real bandwidth, not hypothetical, since every player
   *and spectator* in a session gets their own copy every tick. Emitting
   only the cells where `cellStates[r][c] == "move"` as a JSON array —
   ```json
   "activeMoves": [{"from":{"row":1,"col":4},"to":{"row":3,"col":4},"progress":0.42}]
   ```
   — costs nothing new to compute (the filter condition already exists) and
   keeps typical-case payload growth under ~200 bytes/tick, since per-piece
   cooldowns bound how many pieces are ever mid-slide at once.
2. **Reuse `GameSnapshot::moveTargets`/`moveProgress` as-is client-side** —
   no new parallel type. `GameStateDeserializer` already owns turning wire
   JSON into a `GameSnapshot` (H2); it gains one more field to populate
   instead of default, same shape as every other field it already handles.
3. **No `RealTimeArbiter`/`GameEngine` changes.** The data this plan needs
   is already computed every tick for `cellStates` — this is purely a
   serialization/deserialization scope change, consistent with
   `CLAUDE.md`'s "keep separation of concerns" rule (nothing about arbiter
   timing/legality changes).
4. **No client-side smoothing/extrapolation work needed.** Confirmed by
   reading `BoardView::render()` (`src/renderer/Board_view.cpp:73-96`): it
   recomputes a piece's slide position from `snap.moveProgress[r][c]`
   fresh on *every* call, statelessly — it never assumes a fixed cadence
   between snapshots (that's `cell.anim.update(dt_ms)`'s job, a separate,
   unrelated sprite-frame clock). So Online Play's ~16-30ms network-tick
   cadence (per H5's own scripted-verification notes) needs no new
   interpolation logic on the client — real `progress` values from the
   wire will render correctly as soon as they stop being defaulted to
   `0.0`. This is a genuine reuse of existing Local Play code, not new
   surface area.

---

## Task breakdown

Continues the graphics-networked-client-plan's own H1–H8 letter sequence,
since this is explicitly that plan's own deferred Decision 1 being revisited.

| Task | What | Depends on | Tests |
|---|---|---|---|
| **I1** ✅ | Extended `GameStateSerializer::serialize()` (new file-private `activeMovesToJson()`) to emit the sparse `activeMoves` array described in Decision 1 above, filtered on `cellStates[r][c] == "move"` (reusing the already-computed condition, no new server-side bookkeeping) — bounds-checked against `moveTargets`/`moveProgress` independently of `cellStates` since a hand-built `GameSnapshot` (tests) may leave those vectors empty or a different size. Updated the class-level doc comment in `include/server/GameStateSerializer.hpp` — the old `moveTargets`/`moveProgress`/`selected` bullet is split: `selected` stays excluded (reworded to say *why* — per-connection UI-gesture state, not shared game state), `activeMoves` moves up to the "Included" list; `captureFlashes` stays excluded, unchanged (still out of scope, see Goal). | — | Updated `tests/test_game_state_serializer.cpp`'s `"serialize emits exactly the approved field set, nothing more"` to include `"activeMoves"`. Added 2 new `TEST_CASE`s: an all-idle snapshot produces an **empty** `activeMoves` array (the sparse-encoding contract itself, asserted directly, not just inferred); a mid-move snapshot (`cellStates[0][0] == "move"`) produces exactly one entry with the correct `from`/`to`/`progress`. `run_tests.exe` 263/263 (was 261, +2 new) — built via the same manual `g++` invocation mirroring the Makefile prior H-tasks used (`make` still isn't installed on this machine). Also rebuilt `kungfu_server`/`kungfu_server_tests` (CMake/Ninja/MSYS2 — `GameStateSerializer.cpp` is shared by both the Makefile and this build) and reran `ctest` (`kungfu_server_tests`, 1/1 passed) to confirm zero regression on the layer I3 will later add real-socket coverage to. `KungFuChess` (MSVC/CMake) not rebuilt — `src/server/*.cpp` isn't part of that target's `ENGINE_SOURCES` glob (confirmed by reading `kungfu-graphics/cpp/CMakeLists.txt`), so this task has no compile surface there at all. |
| **I2** | Extend `GameStateDeserializer::deserialize()` (`src/net_client/GameStateDeserializer.cpp`) to parse `activeMoves` back into `GameSnapshot::moveTargets`/`moveProgress` at the right board indices, leaving every other cell at the existing sentinel default (`Position{-1,-1}`/`0.0`) it already uses today. Must still tolerate a missing/absent `activeMoves` key without throwing (an older server, or any other reason the key isn't present) — same tolerance pattern every other optional field in this deserializer already follows via `j.value(...)`. | I1 | Split `tests/test_game_state_deserializer.cpp`'s `"deserialize defaults the fields the wire protocol never sends"` (line 95) into two cases: key absent → still defaults (the tolerance case), key present with 1-2 entries → real values land at the correct `[row][col]` indices, everything else stays at the sentinel default. |
| **I3** | Real-server verification at the ctest/real-socket layer (`server/tests/test_websocket_server.cpp`, G2's target — the layer that actually proves the claim against a live server, not just the pure serializer/deserializer unit tests): a real in-flight move produces a non-empty `activeMoves` entry over an actual broadcast tick; an idle board (no pending moves) produces an empty array on the wire. | I1 | New `ctest` case(s) via the same real-WebSocket-connection helpers `test_websocket_server.cpp` already uses. |
| **I4** | Wire verification on the graphics client: once I2 populates real `moveTargets`/`moveProgress` instead of defaults, `BoardView::render()`'s existing slide-interpolation code (Decision 4 above — already proven, Local-Play-only code, now exercised by Online Play too for the first time) should render sliding pieces with no client-side code changes at all. This task is verification, not new code: confirm via the same scripted `OnlineClient`-driving approach H4–H7 used (a real in-flight move's `pollGameState()` result shows non-default `moveProgress` partway through the travel time) plus a manual GUI click-through pass (the user's own, same as H3a–H7) to confirm pieces visibly slide instead of snap. | I2, H5 | Scripted `OnlineClient` check (no GUI) + manual click-through (GUI) — same split H4–H7 already used, no new pure logic to doctest here. |
| **I5** | Docs: update `graphics-networked-client-plan.md`'s own Decision 1 (which explicitly deferred this) to point at this plan as its resolution, and update README.md's Online Play section (H8) to drop the "pieces snap between ticks" framing once I1–I4 are verified. | I1–I4 | N/A — documentation only, same bar as H8: every claim re-verified against the actual behavior before being written down. |

---

## Out of scope (explicitly, so it doesn't creep in)

- **`captureFlashes`** — the other field `graphics-networked-client-plan.md`'s
  Decision 1 excluded. Not touched by this plan; a separate future task if
  it turns out to matter.
- **`selected`** — already reconstructed client-side from
  `NetworkClickHandler::pendingSelection()` (H5), not a wire concern at all,
  untouched here.
- Any change to `RealTimeArbiter`/`GameEngine` timing or legality — this
  plan is a serialization-layer change only (Decision 3 above).

## Resuming this plan

Next step when picked back up: **I1**, once the task breakdown above is
reviewed and confirmed. Nothing in `GameStateSerializer.hpp`/`.cpp`,
`GameStateDeserializer.hpp`/`.cpp`, or the wire protocol itself should
change before that review happens.
