# Kung Fu Chess — wire protocol: moveProgress/moveTargets for smooth Online Play animation (TODO / reference plan)

> **Status: I1 through I4 done.** `GameStateSerializer` emits the sparse
> `activeMoves` array (I1), `GameStateDeserializer` parses it back into
> `GameSnapshot::moveTargets`/`moveProgress` (I2), a real-socket test
> against a live `WebSocketServer` proves both ends over the actual wire
> (I3), and a scripted `OnlineClient`-driven check (I4) confirms real,
> climbing `moveProgress` values reach the networked graphics client's own
> `GameSnapshot` with zero client-side code changes, exactly as Decision 4
> predicted. **The one open item**: I4's own manual-GUI half (confirming
> pieces visibly slide on screen, not just in the polled data) is the
> user's own click-through, same as every H-task before it. Pick up at
> **I5** (docs) once that's done, or sooner if the user wants to proceed
> without waiting on it.

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
| **I2** ✅ | Extended `GameStateDeserializer::deserialize()` (new file-private `applyActiveMoves()`) to parse `activeMoves` back into `GameSnapshot::moveTargets`/`moveProgress` at the right board indices, leaving every other cell at the existing sentinel default (`Position{-1,-1}`/`0.0`) it already used before this task. Tolerates a missing/absent `activeMoves` key (older server, or any tick with nothing mid-move) without throwing — same `j.value(...)`-style tolerance every other optional field in this deserializer already follows — and also skips (rather than throws on) a malformed individual entry: missing `from`/`to`, non-object `from`/`to`, or an out-of-bounds row/col. Updated both the header's and this task's own doc comments: `moveTargets`/`moveProgress` moved out of the "never round-trips" list into their own paragraph describing the I1/I2 relationship; `selected`/`captureFlashes` stay in the "never round-trips" list, reworded to explain *why* each individually rather than lumping all three under one blanket excluded-fields rationale. | I1 | Split `tests/test_game_state_deserializer.cpp`'s `"deserialize defaults the fields the wire protocol never sends"` into three: the original (now scoped to just `selected`/`captureFlashes`/`whiteName`/`blackName`, which are still unconditionally defaulted), a new case for `activeMoves` absent (still sentinel-defaults, sized to the board), and a new round-trip case (a real `activeMoves` entry lands at the correct `[row][col]`, every other cell stays at the sentinel). Also added a malformed-entry case (out-of-bounds row/col skipped, not thrown). `run_tests.exe` 266/266 (was 263, +3 new). `KungFuChess` (MSVC/CMake) rebuilt clean too this time — unlike I1, `src/net_client/*.cpp` **is** part of that target's own source glob (`NET_CLIENT_SOURCES` in `kungfu-graphics/cpp/CMakeLists.txt`), confirmed by a real compile+link, not just by reading the glob. |
| **I3** ✅ | Real-server verification at the ctest/real-socket layer (`server/tests/test_websocket_server.cpp`, G2's target). New `TEST_CASE("WebSocketServer: activeMoves is empty at rest and reports a real in-flight move's progress")`: matches two real clients, confirms the starting-position broadcast's `activeMoves` is present but empty (nothing mid-move yet), sends a real `WPe2e4`, polls for a tick reporting `0 < progress < 1` for the `{6,4}→{4,4}` cell (real travel time, ~2s simulated, fed from real elapsed ticks — polled for, not assumed at a fixed timestamp, same convention every other timing-sensitive case in this file already uses), then confirms the *landed* broadcast (`board[4][4] == "wP"`) no longer reports that cell in `activeMoves` — proving this is a live per-tick signal tied to `cellStates=="move"`, not something that gets stuck on once set. | I1 | New `ctest` case via the same real-WebSocket-connection helpers (`login`/`waitForBoardMatching`) this file already uses, plus one new file-local predicate (`activeMovesHasE2e4MidFlight`). Ran standalone (`kungfu_server_tests.exe --test-case=...`) to confirm 1/1 passed, 8/8 assertions — not just "the suite as a whole didn't fail." Full suite: `ctest --test-dir server/build` 1/1 (the whole doctest binary is one ctest entry), 14s. |
| **I4** ✅ (scripted half) | Wire verification on the graphics client — no client-side code changed, per Decision 4. Confirmed via a throwaway (not committed) console program linking `OnlineClient` directly (`kungfu-graphics/cpp/src/`, no OpenCV needed — same no-GUI approach H4–H7 used), driven through a temporary CMake target added to `kungfu-graphics/cpp/CMakeLists.txt` for the build, then reverted (`git checkout --`) once verified. **Real finding along the way**: the very first tick reporting `cellStates[6][4] == "move"` can legitimately show `moveProgress == 0.0` (the scheduling tick itself) — a single-sample "0 < progress < 1" assertion is flaky by construction, not a bug in the product code. Fixed by sampling repeatedly over a real window and asserting progress actually climbs (`0.000 → 0.303` in the passing run), which is what `BoardView::render()`'s per-frame interpolation actually depends on. | I2, H5 | Scripted program: logged in, created a room alone (immediate starting-position broadcast, confirmed idle at e2), sent `WPe2e4`, sampled `pollGameState()` until `moveProgress[6][4]` climbed past 0.3 while `moveTargets[6][4]` correctly read `(4,4)` throughout, then confirmed the move resolved (`board[4][4] == "wP"`). All 4 checks passed against a freshly-launched `kungfu_server.exe`. `KungFuChess` (MSVC/CMake) reconfigured + rebuilt clean after the scratch target/CMake edit/source file were fully removed, confirming zero residue. **Not yet done**: the manual GUI click-through (pieces visibly sliding, not the underlying polled data) — the user's own, same category of item every H-task from H3a on deferred to them. |
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
