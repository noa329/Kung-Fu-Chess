# Kung Fu Chess

A real-time chess variant in C++17: there are no turns — both colors can
move concurrently, and moves take real travel time to resolve instead of
happening instantly. The rules engine is shared by four separate binaries:
a headless test/text-protocol build, a real OpenCV graphics client, and a
WebSocket multiplayer server + shell client.

See `CLAUDE.md` for the full architecture writeup (the 8+ layer design,
why includes are flat, VPL submission constraints, etc.) and
`docs/tasks/` for the design history behind each feature. This file is
just build/run instructions for the four binaries.

## 1. Tests / text-protocol binary — `run_tests.exe`

The doctest suite (200+ cases) covering every engine/server-logic layer
except the raw WebSocket glue (see `kungfu_server_tests` below for that).

```sh
make               # builds run_tests.exe
./run_tests.exe                                    # run everything
./run_tests.exe --test-case="name of a test case"  # run one case
./run_tests.exe -ltc                                # list all case names
make clean         # removes run_tests.exe and coverage files
```

Requires a `g++`/`gcc` toolchain on `PATH` (MSYS2/ucrt64 on Windows — run
this from an MSYS2 UCRT64 shell, not plain PowerShell, since `make` isn't
on PowerShell's default `PATH`). If `make` itself isn't installed, the
Makefile's rule can be run by hand instead — see `Makefile` for the exact
`SOURCES`/`INCLUDES` it computes.

There's no separate non-test build via `make` — `TARGET` is set twice in
the Makefile, and `run_tests.exe` is what it actually produces. To run the
text-protocol binary itself (reads a `Board:`/`Commands:` script from
stdin), compile `main.cpp` + `src/*/*.cpp` (excluding `tests/`) with the
same include paths instead.

## 2. Graphics client — `KungFuChess.exe`

A real OpenCV window with mouse input, driven by the same engine classes
as everything else here.

1. Download the `OpenCV_451` folder (link in
   `kungfu-graphics/cpp/README.md`) and place it at
   `kungfu-graphics/cpp/OpenCV_451/` (gitignored, not checked in).
2. Open the repo root in VS Code. `.vscode/settings.json` already points
   `cmake.sourceDirectory` at `kungfu-graphics/cpp`, so the CMake Tools
   extension configures/builds this target directly — build and run
   `KungFuChess` from there.

This build is pinned to **MSVC**, not MinGW (OpenCV_451's prebuilt `.lib`
is MSVC-ABI only) — unlike the server below, which deliberately avoids
MSVC. Asset/sound paths are baked in as absolute compile-time defines, so
the built `.exe` can be run from anywhere, not just the repo root.

Launching `KungFuChess.exe` always opens a **Local Play / Online Play**
chooser first (click a button, or ESC to quit). Local Play is the
self-contained single-window game against a `GameEngine` this binary owns
itself — no server needed. Online Play (below) is additive; picking it
never affects Local Play's own behavior.

### Online Play

Plays a real game over the wire against `kungfu_server.exe` (step 3 below)
— the same `GameEngine`/`Controller`/`BoardView`/`HudView` classes as Local
Play, just fed from the network instead of an in-process engine. **Start
the server first**: `KungFuChess.exe` connects to a hardcoded
`ws://127.0.0.1:9002/` (see `SERVER_URI` in `kungfu-graphics/cpp/src/main.cpp`)
with no command-line option to point it elsewhere.

1. Click **Online Play** at the chooser screen. It connects, then shows a
   login screen: **Username**/**Password** fields (Tab switches field,
   Enter submits, ESC goes back). Any not-yet-seen username auto-registers
   — same auth as `kungfu_client.exe` below, since both talk to the same
   server.
2. At the menu, click **Quick Match** (paired with whoever else is
   searching), **Create Room** (get a shareable room ID — the screen stays
   up showing it until an opponent actually joins, or you press any key to
   view the board early), or **Join Room** (enter a room ID someone else
   created; joining a room that already has two players makes you a
   spectator instead of a player).
3. Once matched, the real board renders from network state. **Left-click**
   a piece, then left-click a destination square to move it (two clicks,
   combined into one wire command); **right-click** a piece to jump.
   **Press `R` to resign** (no-op for a spectator). A disconnected
   opponent shows a live "auto-resign in Ns" countdown on their name bar,
   clearing automatically if they reconnect within 20s. **ESC** disconnects
   and returns to the Local/Online chooser.

Run two `KungFuChess.exe` instances (or one `KungFuChess.exe` plus one
`kungfu_client.exe`, step 4) against the same running server to actually
play a game — same as the shell client below.

## 3. Multiplayer server — `kungfu_server.exe`

A headless WebSocket server (login/matchmaking/rooms/spectators,
disconnect handling with reconnect, SQLite-backed accounts). Builds via
CMake on the **MSYS2/ucrt64** toolchain (Ninja, not MSVC — this build has
no OpenCV dependency, so there's no reason to pull in MSVC's toolchain).

```sh
cmake -S server -B server/build -G Ninja
cmake --build server/build
./server/build/kungfu_server.exe
```

Run it **from the repo root** — it loads `boards/standard.txt` for the
starting position and creates a gitignored `data/kungfu_chess.db`
(SQLite, accounts) and `server.log` alongside wherever it's launched from.
Listens on port `9002`.

### Automated server tests — `ctest`

The same build also produces `kungfu_server_tests`, real-socket tests of
the WebSocket glue itself (login, matchmaking, rooms/spectators,
disconnect/reconnect, resign, discrete event pushes) that the Makefile
build can't reach (it needs `websocketpp`/Asio, which `run_tests.exe`
deliberately doesn't depend on):

```sh
cmake --build server/build
ctest --test-dir server/build --output-on-failure
```

## 4. Multiplayer client — `kungfu_client.exe`

A text-only shell client for the server above: prompts for
username/password (auto-registers a new username), then a menu to quick
match, create a room, or join one by ID. Prints the board via the same
`BoardPrinter` the text-protocol binary uses. Built by the same CMake
invocation as the server (step 3) — no separate configure needed.

```sh
./server/build/kungfu_client.exe                    # connects to ws://127.0.0.1:9002/
./server/build/kungfu_client.exe ws://host:port/     # or a specific server
```

Run two instances (against the same running `kungfu_server.exe`) to
actually play a game.

## Course VPL submission

```sh
scripts/build_vpl_submission.sh   # produces kung_fu_chess_vpl.zip
```

Copies every `include/**/*.hpp` + `src/**/*.cpp` + `main.cpp` into one
flat directory and zips it — the course's VPL grader accepts a flat zip
with no subfolders and doesn't add per-directory `-I` flags (this is also
why every `#include` in this codebase is a bare filename, not a
layer-qualified path — see `CLAUDE.md`/`docs/ARCHITECTURE_DECISIONS.md`
if you're tempted to "clean that up").
