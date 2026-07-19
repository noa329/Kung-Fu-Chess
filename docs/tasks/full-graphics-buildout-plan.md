# Kung Fu Chess — graphics UI build-out: full plan for Claude Code

## How to use this file

Read this whole file before touching any code. Work through the stages
**in order** — each one depends on the previous. One commit per stage
minimum (split further if a stage still feels like "more than one concern").
After each stage: confirm it builds, run a quick headless check (pattern
described at the bottom), *then* commit, *then* move on.

**Important:** some of this may already partially exist in this repo (I made
a local edit myself to the capture-flash-color commit already). Before
writing a stage's code, check whether something matching its "Desired
behavior" already exists. If it does, diff it against this spec, keep
whatever's already correct, and ask me before overwriting anything that
looks like real work rather than a stub. Don't assume the repo is empty.

Architecture ground rules, conventions, and the full data-flow summary are
in `CLAUDE.md` at the repo root — read that too, it's short on purpose.

---

## Stage 1 — `Img`: fix a real bug + add what a game loop needs

**Bug to fix:** `draw_on`'s alpha-blend mixed a per-pixel alpha *matrix*
(`h x w`) with a single *column* of the target ROI (`roi.col(c)`, `h x 1`) —
only worked by accident on 1px-wide images, throws a `cv::Exception` in
`gemm` on any real sprite. Fix: split source into B/G/R/A planes, convert
alpha to `CV_64F`, blend each full-size color plane against the full-size
alpha plane, merge back. Also: the original also converted a 4-channel
source down to 3 channels when the target had no alpha, silently destroying
transparency in the (common) case of an opaque board + transparent piece —
don't do that; blend using the source's own alpha regardless of the
target's channel count, and leave the target's own alpha channel (if any)
untouched.

**Additions needed** (game loop can't work without these):
- `Img(const Img&)` / `operator=` that deep-copy (`cv::Mat`'s default copy
  is shallow/refcounted — breaks "render a fresh frame every tick").
- `clone()` — explicit deep copy.
- `width()` / `height()`.
- `rectangle(x, y, w, h, color, thickness)` — for the selection highlight.
- `circle(cx, cy, radius, color, thickness)` — for the capture-flash effect
  (stage 4). Same reasoning as `rectangle`: effects draw through `Img` too,
  never raw OpenCV calls elsewhere.
- `show_frame(window_name, delay_ms)` — non-blocking, doesn't destroy the
  window afterwards (the existing `show()` does both — keep it for one-off
  tests, add this instead of changing it).
- `on_mouse(window_name, callback, userdata)` — `cv::namedWindow` +
  `cv::setMouseCallback` in one call, so mouse handling stays routed
  through `Img` like everything else.

---

## Stage 2 — `RealTimeArbiter`: expose what timing state exists

Add, without changing existing move/jump/capture behavior:

- `PendingMove` gets a `startTime` field (alongside the existing
  `arrivalTime`).
- `getMoveProgress(pos, out)` — if `pos` has a pending move, fills `out`
  (`to` + `progress` 0..1 = `(currentTime - startTime) / (arrivalTime -
  startTime)`, clamped) and returns true. Lets a view interpolate a piece's
  on-screen position instead of only snapping at arrival.
- A `RestingState { Position pos; long long endTime; bool isLongRest; }`
  list (`restingPieces`), populated when a move/capture lands (→ long rest)
  or a jump ends (→ short rest), pruned in `advance()` the same way
  `airbornePieces` already is.
- `isResting(pos, bool* out_isLongRest = nullptr)` — true while `pos` is in
  either cooldown; `out_isLongRest` tells which one.
- Two constants for now: `LONG_REST_MS = 800`, `SHORT_REST_MS = 500`
  (placeholders — stage 6 replaces these with config-derived values, don't
  try to make them config-driven yet in this stage).

---

## Stage 3 — `GameEngine`: enforce cooldown + expose it

- `isMovementLegal` additionally rejects if `arbiter.isResting(from)`.
- `jump()` additionally rejects if `arbiter.isResting(pos)`.
- `GameSnapshot` gains:
  - `cellStates` (`vector<vector<string>>`): `"idle"` / `"move"` / `"jump"`
    / `"short_rest"` / `"long_rest"`, per occupied cell, derived from
    `arbiter.getMoveProgress`/`isAirborne`/`isResting`.
  - `moveTargets` (`vector<vector<Position>>`) + `moveProgress`
    (`vector<vector<double>>`) — only meaningful where `cellStates == "move"`.
  - `captureFlashes` — see stage 4.
- **Desired behavior to verify headlessly:** land a move → state flips to
  `long_rest` at the exact arrival frame; attempt to select/move that piece
  again → silently rejected (piece doesn't move, stays wherever it was);
  after `LONG_REST_MS` passes → state flips to `idle` and the same move now
  succeeds. Same pattern for jump → `short_rest`.

---

## Stage 4 — capture flash (visual only, no gameplay effect)

`CaptureEvent` already exists (from `arbiter.advance()`) but historically
only affected `gameOver` (via `wasKing`) — the capture itself had zero
visual trace. Add:

- `GameEngine` gets an internal clock (`clock_`, incremented in `wait(ms)`)
  and a pruned list of active captures (start/expire time,
  `CAPTURE_EFFECT_MS = 400` as a first value — feel free to tune), fed by
  `CaptureEvent`s each `wait()` call.
- `GameSnapshot::captureFlashes`: `{ Position at; char capturedColor; bool
  wasKing; double progress; }` (0 = just happened, 1 = about to disappear),
  for every still-active capture.
- `BoardView::render` draws one ring per flash via `Img::circle` — radius
  and thickness grow with `progress`, color shifts (e.g. orange → red).
  Captured king can read as slightly bigger/thicker. This is purely
  decorative — it must not affect `boardTokens` or any gameplay state.

**I already made a local edit to this specific piece (the color of the
ring) — check what's there before rewriting it.** Keep my color choice if
it's already sensible; the important part is the mechanism (progress-driven
ring), not the exact hex values.

---

## Stage 5 — `renderer`: `SpriteAnimation` / `PieceAnimator` / `BoardView`

All in `include/renderer` + `src/renderer`, all drawing exclusively through
`Img`. If these already exist from earlier work, treat this as the spec to
diff against rather than a green field.

- **`SpriteAnimation`**: loads one state folder (e.g.
  `pieces1/PW/states/idle`) — numbered frames from `sprites/1.png, 2.png,
  ...` resized to a given cell size, plus `frames_per_sec` / `is_loop` /
  `next_state_when_finished` from that folder's `config.json`. `update(dt_ms)`
  advances a frame index by elapsed time; returns true the instant a
  non-looping animation finishes (so the owner can switch to
  `next_state_when_finished`).
- **`PieceAnimator`**: owns all 5 states (`idle`/`move`/`jump`/`short_rest`/
  `long_rest`) for one on-board piece instance, runs the transition between
  them per `SpriteAnimation`'s finished signal.
- **`BoardView`**: **pure view, owns no game logic.** Each frame:
  1. `syncFromSnapshot(snap)` — for every occupied cell, ensure a
     `PieceAnimator` exists for the right sprite code (map engine token
     `<color><TYPE>` e.g. `"wP"` to sprite folder `<TYPE><COLOR>` e.g.
     `"PW"` — write this mapping once, it's used constantly), keyed by
     board cell `(row, col)` in a cache that survives across frames (a
     piece "teleporting" from origin to destination on arrival must not
     lose/reset its animator inappropriately — reload only when the code at
     that cell actually changes). Push each cell's target state from
     `cellStates` into its animator.
  2. `update(dt_ms)` — advance every cached animator.
  3. `render(snap)` — clone the board image, draw each piece's current
     frame. For cells whose state is `"move"`, don't draw at the fixed
     cell position — lerp the pixel position between the origin cell and
     `moveTargets[r][c]` by `moveProgress[r][c]` (this is what makes
     movement slide instead of teleport). Draw the selection highlight
     (`snap.selected`) and capture flashes (stage 4) last.
- Clicks are **not** handled inside `BoardView` — route them through
  `Controller::handleClick`/`handleJump` from `main()`'s mouse callback.

---

## Stage 6 — wire it all together

- `kungfu-graphics/cpp/CMakeLists.txt`: compile the engine sources
  (`model`, `movement_rules`, `real_time_arbiter`, `rule_engine`,
  `controller`, `game_engine`) and the `renderer` sources into the same
  OpenCV-linked executable (none of the engine sources need OpenCV — this
  is just so `main()` can use the real `GameEngine`/`Controller` instead of
  a graphics-only demo). Add a `KUNGFU_ASSETS_ROOT` compile definition set
  to an **absolute** path to `kungfu-graphics/` (`${CMAKE_CURRENT_SOURCE_DIR}/../`)
  — don't rely on a relative path guessing the working directory the .exe
  happens to be launched from, that silently breaks depending on how it's run.
- `kungfu-graphics/cpp/src/main.cpp`: build a `GameEngine`, load a standard
  starting position, wrap it in a `Controller`. Register
  `Img::on_mouse(window, onMouse, &controller)` — left click →
  `controller.handleClick(x, y)`, right click → `controller.handleJump(x, y)`.
  Loop: `engine.wait(dt)` → `engine.snapshot()` →
  `view.syncFromSnapshot(snap)` → `view.update(dt)` →
  `view.render(snap).show_frame(window, ~16)`. Use `KUNGFU_ASSETS_ROOT`, not
  a hardcoded relative path.
- Watch for case-sensitive `#include` mismatches (e.g. `"board_view.hpp"`
  vs an actual file `Board_view.hpp`) — invisible on Windows, breaks on
  Linux/Mac. Match actual filenames exactly.

---

## Stage 7 — config-driven rest durations (current active task)

Full spec already written: `docs/tasks/config-driven-rest-durations.md`.
Short version: `LONG_REST_MS`/`SHORT_REST_MS` from stage 2 were guesses
(800/500ms) — every piece's `short_rest`/`long_rest` config.json + its
sprite frame count gives the *real* duration
(`frame_count / frames_per_sec * 1000`; measured on `pieces1`:
short_rest ≈ 625ms, long_rest ≈ 833ms, consistent across the piece types
checked). Read that file for the constraint (engine layer must stay
buildable by the plain Makefile with no OpenCV/filesystem dependency) and
the suggested approach before starting.

---

## Stage 8 — player names, score, moves log (HUD around the board)

Full spec: `docs/tasks/player-names-score-moves-log.md`. Explicit
constraint from the person: **no changes to move/legality logic** -
everything here is either new data derived from events that already exist
(scores from `CaptureEvent` + a piece-value lookup, names, a move-history
list) or a new rendering layer that wraps `BoardView::render()` from the
outside rather than changing it. Read that file for the exact layout
(matches a reference screenshot: side panels with per-player move logs,
top/bottom name+score bars, file/rank coordinate labels) and the open
question about long move lists that needs confirming before over-building.

---

## Testing convention (use this for every stage, not just at the end)

For engine/timing changes: write a small throwaway `.cpp` that builds a
`GameEngine`, drives it through `Controller`, calls `engine.wait(ms)` in a
loop, and prints/asserts the values that matter (state strings, progress
numbers, token positions) at the specific milliseconds where behavior
should change. Delete it after. This caught a real crash (stage 1's bug)
that a build-only check would have missed.

For rendering changes: same idea but write frames to PNG
(`cv::imwrite`) instead of opening a window, so it can be checked
headlessly (crop the relevant cell, or count pixels of an expected color,
if eyeballing the PNG isn't enough to confirm the math).
