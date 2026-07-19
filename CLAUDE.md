# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

Kung Fu Chess: real-time chess variant (no turns — both colors can move
concurrently; moves take travel time instead of resolving instantly). C++17.
Originated as a bootcamp/course assignment (VPL submission constraints below
are a direct result of that).

There are **two separate build targets** sharing the same engine code:

1. **Text-protocol binary** (`main.cpp` at repo root + `Makefile`) — reads a
   `Board:`/`Commands:` script from stdin, drives the engine through its public
   API only, prints results. This is what the doctest suite and the course's
   VPL grader exercise.
2. **Graphics binary** (`kungfu-graphics/cpp/`, CMake + OpenCV) — a real
   window with mouse input, wired to the *same* `GameEngine`/`Controller`
   classes from `src/`/`include/` (see "Two entry points" below).

## Global Working Protocol

These govern how to operate in this repo session-to-session, independent of
any specific task:

- **Testing & verification.** Before any commit, do a full build and run the
  appropriate headless tests (`make` + `./run_tests.exe` for engine/logic
  changes; the CMake/OpenCV build plus a headless PNG/assert check per the
  "Testing convention" in `docs/tasks/full-graphics-buildout-plan.md` for
  rendering changes). If anything fails, identify the root cause and fix it
  autonomously before proceeding — don't hand back a red build.
- **Autonomous commits.** On completing a logical task, feature, or
  architectural change, stage the relevant files and commit with a concise,
  descriptive message without waiting to be asked — unless told otherwise for
  that particular piece of work.
- **Architectural integrity.** All UI/graphics modifications must be strictly
  external and non-intrusive to `RealTimeArbiter` game logic; keep separation
  of concerns between layers at all times. See "Critical constraints" below
  for how this and the `Img`-only/strings-not-enums rules apply concretely.
- **Constraint enforcement.** Rendering goes through `Img` only; state is
  represented with strings, not enums (see "Critical constraints" below).
- **Proactive feedback.** If a requirement is ambiguous, ask for
  clarification before attempting a solution — don't guess and build on the
  guess.

## Build & test commands

### Tests / text-protocol binary (Makefile, g++, doctest)

```sh
make               # builds run_tests.exe from src/*/*.cpp + tests/*.cpp
./run_tests.exe               # run the full doctest suite
./run_tests.exe --test-case="name of a SCENARIO/TEST_CASE"   # run one test
./run_tests.exe -ltc          # list all test case names
make clean         # removes run_tests.exe and *.gcda/*.gcno coverage files
```

Note: `TARGET` is set twice in the `Makefile` (first to `kungfu_chess`, then
overridden to `run_tests.exe`) — the tests binary is what `make` actually
produces; there is no separate non-test build via `make`.

To run the text-protocol binary itself instead of tests, compile `main.cpp` +
`src/*/*.cpp` (excluding `tests/`) with the same include paths.

### Graphics binary (CMake + OpenCV, Windows/VS)

Requires the `OpenCV_451` folder (see `kungfu-graphics/cpp/README.md` for the
download link) placed at `kungfu-graphics/cpp/OpenCV_451/` — it's gitignored
and not checked in. `.vscode/settings.json` points `cmake.sourceDirectory` at
`kungfu-graphics/cpp`, so opening the repo root in VS Code + CMake Tools
configures/builds this target directly. The existing `build/` directory is a
generated Visual Studio solution — don't hand-edit it.

### VPL submission (course grader)

```sh
scripts/build_vpl_submission.sh   # produces kung_fu_chess_vpl.zip
```

The course's VPL grader accepts a flat zip (no subfolders) and does **not**
add `-I` flags for subdirectories. The script copies every `include/**/*.hpp`,
`src/**/*.cpp`, and `main.cpp` into one flat directory and zips it. This is
why every `#include` in the codebase uses a bare filename
(`#include "Board.hpp"`) rather than a layer-qualified path
(`#include "model/Board.hpp"`) even though the physical files live in
per-layer folders — a layer-qualified include would compile locally (thanks
to the `Makefile`'s `-I` per subdirectory) but fail in the VPL grader. See
`docs/ARCHITECTURE_DECISIONS.md` ("תיקון VPL") for the full incident history
if you're tempted to "clean up" the includes.

## Critical constraints (graphics/HUD build-out)

These apply to any work under `docs/tasks/full-graphics-buildout-plan.md`,
`docs/tasks/player-names-score-moves-log.md`, and similar follow-on graphics
tasks — violating any of them is a scope error, not a style nitpick:

- **Stick to the layer-based architecture.** New code goes in the layer that
  already owns that responsibility (see below) — don't reach across layers or
  invent a new one to avoid a small refactor. HUD/score/move-log additions
  are `game_engine`-layer data (exposed via `GameSnapshot`) plus a
  `renderer`-layer drawing pass; they are not an excuse to let, say,
  `RuleEngine` know about scores.
- **Enforce the `Img`-only constraint.** All drawing (board, pieces,
  highlights, capture flashes, HUD panels, text, coordinate labels) goes
  through `Img`'s own primitives (`rectangle`, `circle`, `put_text`,
  `draw_on`, etc.). No raw `cv::` calls anywhere outside `img.hpp`/`img.cpp` —
  if a primitive is missing, add it to `Img`, don't reach around it.
- **Use strings for states, not enums.** Cell/animation states
  (`"idle"`/`"move"`/`"jump"`/`"short_rest"`/`"long_rest"`, etc.) are plain
  `std::string`s through `GameSnapshot` and `PieceAnimator`, matching the
  existing `cellStates` convention — don't introduce a C++ `enum`/`enum
  class` for these, even where it'd be the more idiomatic choice.
- **`BoardView` changes must be strictly external and non-intrusive to
  `RealTimeArbiter` game logic.** `BoardView` (and the HUD layer around it)
  only ever *reads* a `GameSnapshot` — it must never reach into
  `RealTimeArbiter`/`GameEngine` internals, and changes to it must never
  require or imply a change to arbiter timing/legality behavior. Data the
  view needs (progress, rest state, capture flashes, scores, move history)
  flows one way: engine → snapshot → view.
- **All rendering changes are confined to the HUD layer.** Per
  `player-names-score-moves-log.md`: `BoardView::render()` itself stays
  exactly as-is (board + pieces + highlight + capture flashes only, verify
  with `git diff`). Names/score/move-log/coordinate-labels are a separate
  compositing pass that calls `BoardView::render()` and draws additional
  panels/bars around the returned image — never edits inside it.

## Known placeholder-art artifact (not a bug)

Rendered frames show text like `"long_rest"` / a frame number baked over
the piece sprite. This is **not** rendered by any code — it's baked
directly into the placeholder sprite PNGs in `kungfu-graphics/pieces1/` and
`pieces2/` (the assets are debug placeholders, not final art) and is known/
expected from early on in this project. It'll disappear on its own once
real art replaces the placeholders — don't spend time investigating it as
a rendering bug.

## Architecture

The engine is deliberately layered into 8 modules, each under matching
`include/<layer>/` and `src/<layer>/` folders, with a strict one-way ownership
model documented in detail in `docs/ARCHITECTURE_DECISIONS.md` (worth reading
before restructuring anything — it records *why* each boundary is where it
is, including reverted mistakes). Summary, roughly bottom-up:

- **`model`** (`Board`, `Piece`/`Pieces`, `Position`, `PieceKind`) — piece
  identity and board storage only. No movement rules, no rendering, no time.
- **`movement_rules`** — pure, stateless geometry (`isValidShape`,
  `isValidCapture`, `isSliding`) per piece kind. Implemented as a Strategy
  pattern: `IMovementStrategy` + one class per piece
  (`KingMovement`/`QueenMovement`/.../`PawnMovement`), dispatched by
  `MovementStrategyFactory` on `PieceKind`. The old `MovementRules` namespace
  is now a thin facade over the factory — its public API is unchanged, so add
  a new piece by adding a new strategy class, not by touching the facade or
  `RuleEngine`.
- **`rule_engine`** (`RuleEngine`) — read-only "is this requested move legal"
  check (shape via `MovementRules` + path-clear via `Board::isPathClear`).
  Knows nothing about pending moves or real time.
- **`real_time_arbiter`** (`RealTimeArbiter`) — owns everything about
  concurrent, in-flight motion: `PendingMove`/`AirborneState`, simulated
  `currentTime`, `scheduleMove`/`scheduleJump`, and `advance(ms)` which
  resolves arrivals and returns `CaptureEvent`s. It reports captures but does
  **not** decide game-over — that's `GameEngine`'s call.
- **`game_engine`** (`GameEngine`) — the public API surface: `select(Position)`
  / `jump(Position)` / `wait(ms)` / `snapshot() const`. No pixels, no
  rendering, no stdin/stdout. `GameSnapshot` is the one read-only view
  everything downstream (Renderer, graphics view, tests) consumes.
- **`controller`** (`Controller`) — the *only* place in the whole project that
  converts pixel coordinates to board `Position` (`pixelToGrid`, private).
  Holds no chess/timing logic of its own; just translates and forwards to
  `GameEngine`.
- **`renderer`** (`Renderer`, plus `Board_view`/`Piece_animator`/
  `Sprite_animation` used by the graphics build) — draws from a
  `GameSnapshot` only. `Renderer` itself is a text/console fallback (this
  project has no bundled 2D drawing lib at the base-repo level); the real
  OpenCV drawing lives in `kungfu-graphics/`. Don't confuse `Renderer`
  (human-readable visual output) with `BoardPrinter` (exact logical-token
  output, see below) — they serve different consumers on purpose.
- **`text_io`** (`BoardParser`, `BoardPrinter`) — parses the `Board:` section
  of a script into tokens (with `ERROR UNKNOWN_TOKEN`/`ERROR
  ROW_WIDTH_MISMATCH`), and prints tokens back out in the exact original
  format. No movement rules, no command execution.
- **`text_test_runner`** (`TextTestRunner`) — parses/runs the `Commands:`
  section (`click`/`jump`/`wait`/`print board`) against `Controller`/
  `GameEngine`'s public API only, so it's testable with `istringstream`/
  `ostringstream` instead of a real process.

`main.cpp` (repo root) is intentionally a ~20-line wire-up: `BoardParser` →
`GameEngine` → `TextTestRunner`. No game logic lives there.

### Two entry points, one engine

- `main.cpp` (root): text protocol, `Controller`/`GameEngine` driven by
  `TextTestRunner` from stdin/stdout.
- `kungfu-graphics/cpp/src/main.cpp`: real OpenCV window. Its `CMakeLists.txt`
  compiles the *same* `model`/`movement_rules`/`rule_engine`/
  `real_time_arbiter`/`controller`/`game_engine`/`renderer` sources from the
  main tree (see the `ENGINE_SOURCES`/`RENDERER_SOURCES` globs) alongside its
  own `img.hpp`/`img.cpp` (OpenCV wrapper) and `Board_view`. Mouse events
  (`EVENT_LBUTTONDOWN` → `Controller::handleClick`, `EVENT_RBUTTONDOWN` →
  `Controller::handleJump`) drive the identical `GameEngine`/`Controller`
  classes used by the text protocol — only the input/output layer differs.

### Board token format

Grid cells are two-character strings: color (`w`/`b`) + piece letter
(`P`/`N`/`B`/`R`/`Q`/`K`), or `.` for empty (see
`standardStartingPosition()` in `kungfu-graphics/cpp/src/main.cpp` for a
worked example).
