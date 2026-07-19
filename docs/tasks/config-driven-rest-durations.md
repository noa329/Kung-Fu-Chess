# Task: make rest-cooldown durations config-driven

## Context (read the files, don't ask me to re-explain them)

`RealTimeArbiter` already enforces a real gameplay cooldown after a piece
lands a move/capture (`long_rest`) or a jump (`short_rest`) - it can't be
selected or moved again until the cooldown ends. See:
- `include/real_time_arbiter/RealTimeArbiter.hpp` - `LONG_REST_MS = 800`,
  `SHORT_REST_MS = 500` (hardcoded placeholders), `isResting()`.
- `src/real_time_arbiter/RealTimeArbiter.cpp` - `restingPieces`, `finalizeResting()`.
- `src/game_engine/GameEngine.cpp` - `isMovementLegal`/`jump` reject a
  resting piece; `snapshot()` reports `"long_rest"`/`"short_rest"`.

This was verified working end to end (see commits `f8786d1`, `5b061b1`).

## The gap

`800`/`500` were guesses. Every piece's sprite folder already specifies real
timing for these states via `states/short_rest/config.json` and
`states/long_rest/config.json` (e.g. `kungfu-graphics/pieces1/PW/states/
short_rest/config.json`): `graphics.frames_per_sec` + the sprite frame count
(count of `sprites/*.png` in that folder) is the *real* duration:
`duration_ms = round(frame_count / frames_per_sec * 1000)`.

Checked across `PW`/`QW`/`KW`/`NW` in `pieces1` - all uniform:
- `short_rest`: 5 frames @ 8fps = **625ms** (current guess: 500ms)
- `long_rest`: 5 frames @ 6fps = **~833ms** (current guess: 800ms)

Confirm this holds for every piece type in both `pieces1` and `pieces2`
before assuming a single global value is enough - if any piece differs,
say so and propose per-piece-type durations instead (bigger change: it'd
need `RealTimeArbiter` to key durations by piece code, not one constant).

## Constraint that shapes the design

`model`/`real_time_arbiter`/`game_engine` must keep building with the plain
root `Makefile` (no OpenCV, no filesystem dependency on `kungfu-graphics/`) -
that's what the standalone logic tests use. So `RealTimeArbiter` itself
should *not* read `config.json` or scan sprite folders.

**Suggested approach** (adjust if you find something cleaner, but check with
me before a bigger restructure):
1. Give `RealTimeArbiter` (or `GameEngine`) a setter, e.g.
   `setRestDurations(long long longRestMs, long long shortRestMs)`, called
   optionally. If never called, keep the current hardcoded values as
   fallback - so existing tests and the plain Makefile build behave exactly
   as before.
2. Add a small, dependency-light reader (`<filesystem>` + the same
   regex-based `config.json` parsing `SpriteAnimation::parseConfig` already
   does in `src/renderer/Sprite_animation.cpp` - reuse that logic rather
   than re-deriving it, ideally by extracting a shared tiny helper both call,
   rather than copy-pasting the regexes) that computes the two durations
   from one representative piece folder (e.g. `pieces1/PW`).
3. Call it once from `kungfu-graphics/cpp/src/main.cpp`, right after
   `BoardView::init`, using the same `KUNGFU_ASSETS_ROOT` the view already
   uses - before the game loop starts.

## Acceptance criteria

- [ ] Root `make` build still succeeds with zero OpenCV/filesystem dependency
      added to `model`/`movement_rules`/`rule_engine`/`real_time_arbiter`/`game_engine`.
- [ ] Graphics app computes long/short rest durations from `pieces1`'s config
      at startup instead of using the hardcoded constants.
- [ ] If `pieces1` is ever missing/unreadable at runtime, falls back to the
      current hardcoded constants rather than crashing.
- [ ] Headless verification (same pattern as `f8786d1`/`5b061b1`): print the
      computed durations, confirm they match the hand-computed values above
      (625ms / ~833ms), and confirm a resting piece is rejected until exactly
      that many ms have passed, then accepted.
- [ ] One commit per logical step, matching existing message style
      (`git log --oneline`), each with what/why, not just what.
- [ ] Add a short entry to `docs/ARCHITECTURE_DECISIONS.md` explaining the
      setter + fallback approach (or whatever you actually built, if different).

## Open questions - ask me, don't guess

- If frame counts/fps genuinely differ per piece type once you check all of
  them, per-piece-type durations are a bigger change than this doc assumes -
  confirm before doing that.
- If you think the duration should be recomputed per-piece-*instance* later
  (e.g. different pieces resting different lengths for gameplay-balance
  reasons), flag it as a future task rather than building it now - out of
  scope here.

## After this task

Check with me for what's next. Backlog exists (not yet speced in detail):
score/captured-piece display, player names, a moves log (see the original
course slides - `CTD_26__UI__pptx.pdf` if it's still in the repo/uploads),
and a brief visual/audio cue when a click is rejected as an illegal move
(currently silent - see `GameEngine::select`).
