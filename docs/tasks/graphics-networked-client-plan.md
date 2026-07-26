# Kung Fu Chess — graphics client goes networked (TODO / reference plan)

> **Status: in progress, paused after H3a.** This started as a
> reviewed-but-not-started reference plan; **H1 and H3a are now done** (see
> their rows below) — the schedule-critical MSVC risk is resolved, and the
> local-play path is now cleanly isolated behind a mode-select screen with
> zero behavior change. **H3a's manual verification has one open gap**: the
> mode-select screen's rendering is confirmed via a real screenshot, but a
> live click-through into `runLocalGame()` couldn't be confirmed
> (desktop-automation flakiness, not a suspected code issue — see H3a's row)
> — worth a real manual click before building further on top of it. Pick up
> at **H2** (or **H3b**, which only depends on H1) when resuming.

## How to use this file

Same convention as `server-phase-plan.md`/`full-graphics-buildout-plan.md`:
work through tasks **in order** (a task's "Depends on" column means what it
says), one small commit per task, doctest-first where the task has pure
logic to test, manual verification documented where it doesn't (real
sockets and a real OpenCV window aren't practical to doctest, same
reasoning `server-phase-plan.md` already uses for its own networking-glue
tasks). Confirm the existing test suites (`run_tests.exe`, and the graphics
build's own headless-PNG-probe convention from
`full-graphics-buildout-plan.md`) still pass before moving to the next task.

## Goal

Today `kungfu-graphics/cpp/` (the OpenCV window) only ever plays against a
`GameEngine` it constructs and owns itself — it has never spoken to
`kungfu_server.exe`. `client/cli/main.cpp` already proves the wire protocol
end-to-end (login, matchmaking, rooms, moves, resign, discrete sound/
lifecycle pushes) but is text-only. This plan adds a real networked path to
the graphics binary, reusing that same wire protocol and the same
background-thread-owns-the-socket pattern `client/cli` already established,
**without removing the existing local/offline mode**.

---

## Decisions already confirmed (don't re-litigate these)

Resolved during plan review, before any task started:

1. **Render fidelity for networked play**: degraded-fidelity-first. The
   wire protocol's `GameStateSerializer` deliberately excludes
   `moveProgress`/`moveTargets` (per-frame slide interpolation) and
   `captureFlashes` — see that header's own comment. Rather than extending
   the wire protocol now, the networked graphics client's JSON→`GameSnapshot`
   translator (H2) just defaults those fields, so pieces snap between ticks
   instead of sliding and captures show no flash effect. Revisit only if
   the degraded fidelity turns out to matter once it's actually played.
2. **Keep both modes.** Local play (today's `GameEngine`-owned path) is
   **not** replaced — it stays exactly as-is, reachable behind a new
   startup choice. Online play is additive.
3. **Disconnect countdown** (`whiteDisconnectMs`/`blackDisconnectMs` on the
   wire, from Task D4/`GameStateSerializer::DisconnectStatus`): surfaced via
   a **separate, `HudView`-only side-channel struct**, not folded into
   `GameSnapshot`. This keeps `GameStateSerializer.hpp`'s own documented
   boundary intact ("connection-layer state... doesn't belong on the
   read-model" — `GameSnapshot` is also used by local play, which has no
   connection at all).
4. **Password field is masked** (`*` per character), not shown in plaintext.
5. **H1's MSVC risk has no pre-built fallback.** `websocketpp`/Asio were
   proven under MinGW/g++ for `server/` (Phase A), never under MSVC (the
   toolchain `kungfu-graphics/cpp/` builds with, because of OpenCV's
   prebuilt MSVC-ABI `.lib`). If they don't compile clean under MSVC, **stop
   and report back** rather than silently pivoting to a MinGW-graphics-build
   migration or any other workaround — that's a big enough change to need
   its own conversation, not something to pre-plan for a risk that hasn't
   materialized.

---

## Three architectural mismatches driving the task design

1. **`GameSnapshot` carries fields the wire protocol excludes on purpose**
   (`moveTargets`/`moveProgress`/`captureFlashes`) — resolved by decision 1
   above.
2. **`selected` is transient, single-instance server-side state** (mutated
   by two sequential `GameEngine::select()` calls inside one
   `GameSession::handleCommand()`), not "this player has square X
   highlighted" persistent state. The graphics client's two-click gesture
   (click a piece, click a destination) has to become **client-local UI
   state** that only sends one combined wire command
   (`"WQe2e5"`/`"JWPe2"`) once both clicks are gathered — it can no longer
   be two separate `engine.select()` calls the way local play does it.
3. **The disconnect countdown lives outside `GameSnapshot` on purpose** —
   resolved by decision 3 above (separate `HudView`-only struct).

---

## Task breakdown

| Task | What | Depends on | Tests |
|---|---|---|---|
| **H1** ✅ | Got `websocketpp` + Asio + `nlohmann` building inside `kungfu-graphics/cpp/CMakeLists.txt` under **MSVC**. Mirrors `server/CMakeLists.txt`'s `FetchContent_Declare`/`FetchContent_Populate` block exactly, same pinned versions (Asio `asio-1-18-2`, websocketpp `0.8.2`), same `CMP0169=OLD` guard, `third_party/nlohmann` referenced the same way. The proof is a new, deliberately isolated `KungFuChessNetSmokeTest` CMake target (`kungfu-graphics/cpp/src/net_smoke_test.cpp`) — no OpenCV dependency at all, excluded from `${PROJECT_NAME}`'s own `LOCAL_SOURCES` glob via `list(REMOVE_ITEM ...)` so it can't collide with `main.cpp`'s `main()` or put the existing local-play build at risk while this was unproven. **Real, previously-unverified MSVC-specific finding** (not hypothetical — a real build failure, not a guess): MSVC reports `__cplusplus` as the pre-C++11 value `199711L` by default regardless of `/std:c++17` (a documented MSVC quirk — the accurate value needs an opt-in `/Zc:__cplusplus` flag MSVC doesn't set by default). websocketpp's own `common/cpp11.hpp` gates its entire C++11-feature-detection block on `__cplusplus >= 201103L`, so under MSVC that block never fires and `common/random.hpp` falls through to a `#include <boost/version.hpp>` path this repo has no Boost for — confirmed via a real `C1083: Cannot open include file: 'boost/version.hpp'` error. **Fixed** by defining `_WEBSOCKETPP_CPP11_STL_` (a build-system escape hatch websocketpp's own `cpp11.hpp` comment documents for exactly this situation — not a hack) via `target_compile_definitions` on the smoke-test target. No `_WEBSOCKETPP_CPP11_THREAD_` define was needed here (unlike `server/CMakeLists.txt`) — that only bypasses a MinGW-only (`__MINGW32__`/`__MINGW64__`) blanket rule in `common/thread.hpp`, which is never true under MSVC, so it would've been a no-op. | — | Manual: `KungFuChessNetSmokeTest.exe` (built via `cmake --build build --target KungFuChessNetSmokeTest --config Debug`, the same VS 2022/MSVC generator the existing `build/` directory already uses) run against a live `kungfu_server.exe` — printed `connected` then `connection closed`, exit code 0, confirming a real WebSocket handshake round trip, not just a compile pass. `nlohmann::json` also exercised in the same binary (`{"type":"net_smoke_test","uri":"..."}` built and dumped before connecting) to confirm it compiles under MSVC too, since Task H2 needs it. Rebuilt the existing `KungFuChess` target immediately after (`cmake --build build --target KungFuChess --config Debug`) to confirm the new `FetchContent`/target additions caused zero regression to local play — built clean, unchanged. |
| **H2** | Pure JSON→`GameSnapshot` translator (new small layer, e.g. `include/net_client`/`src/net_client`, no OpenCV dependency) that parses the untyped state-broadcast JSON into a real `GameSnapshot` — reusing `GameSnapshot` itself (decision 1) rather than inventing a parallel type, defaulting the wire-excluded fields (`selected={-1,-1}`, `moveProgress`/`moveTargets` zeroed/sized, `captureFlashes` empty) so `BoardView`/`HudView` need **zero** code changes to consume it. Also parses the discrete `sound`/`lifecycle` push messages (Task G4's wire format) into a small typed struct. | — | doctest, no sockets needed (same style as `GameStateSerializer`'s own tests, just inverted direction) — JSON fixture in, `GameSnapshot`/event struct out, field-by-field, including the "excluded fields land at their documented defaults" case. |
| **H3a** ✅ | Safety-first refactor, zero behavior change: extracted today's entire local-play loop (`BoardParser` → `GameEngine` → `Controller` → `BoardView`/`HudView` → `EventBus` wiring) out of `main()` into `runLocalGame()` — body is a byte-for-byte copy of the pre-refactor `main()` (only the `window_name` local became a file-scope `WINDOW_NAME` constant, shared with the new screens below). Added a new top-level **"Local Play" / "Online Play"** mode-select screen (`Img`-only, `chooseMode()`) in front of it, per the two decisions confirmed before starting: **mouse-click buttons** (two `Img::rectangle` buttons, reusing the exact `Img::on_mouse` callback pattern the gameplay screen's own `onMouse()` already established — no new input mechanism needed), and **Online Play is a stub screen that loops back to `chooseMode()`** (`showOnlinePlayStub()` — "Coming soon", any key returns to the menu) rather than a disabled/greyed-out option, since H3b doesn't exist yet but the option should still be fully reachable end-to-end. `main()` is now: `chooseMode()` → loop on `showOnlinePlayStub()`+`chooseMode()` while `"online"` → `return runLocalGame()` once `"local"` is chosen (`"quit"`, i.e. ESC at the menu, exits immediately) — once Local Play starts, it keeps its exact prior top-level behavior including ESC ending the whole program, not returning to a menu (a deliberate, explicit part of "zero behavior change"). No networking dependency at all — built and verified independently of H1/H2. | — | `cmake --build build --target KungFuChess --config Debug` — clean build, zero warnings introduced. **Manual, with an honest limitation**: launched the real `KungFuChess.exe` and screenshotted the live window (via a PowerShell/.NET `Graphics.CopyFromScreen` capture, since no project-specific run-skill or GUI-automation tool exists for this native Win32/OpenCV app) — confirmed the mode-select screen renders correctly: title, both buttons legible and correctly positioned, not blank/black/crashed. **Could not confirm the click-through into `runLocalGame()` itself** — repeated synthetic-mouse-click attempts (via `user32.dll` `mouse_event`/`SetForegroundWindow`) were intercepted by other windows on the live desktop (VS Code, a browser) stealing top-of-z-order between the click and the follow-up screenshot, confirmed by re-screenshotting and finding the mode-select screen still showing (i.e. `chooseMode()`'s own loop was still running - the click never reached `onModeSelectMouse` at all, not a code-side failure). Did not keep fighting environment flakiness on real-desktop GUI automation for a check that code review already answers with certainty: `runLocalGame()`'s body is a direct, uneditorialized copy of the prior `main()`, so its behavior is unchanged by construction, not by a runtime probe. **Recommend a real manual click-through pass (a few seconds, actually clicking "Local Play" and playing a move) before relying on this beyond the diff-level guarantee.** |
| **H3b** | Online-play screens (`Img`-only, new renderer-layer view alongside `HudView`, non-intrusive to `RealTimeArbiter`): login (username + password, password masked per decision 4), post-login menu (quick match / create room / join room), status screens ("connecting…", "searching for an opponent…", room-ID display), retry-on-reject. Text entry via `show_frame`'s returned key code (confirmed unmasked ASCII already — existing code checks `key == 'm'`/`key == 27` directly with no `& 0xFF` needed). | H1 | Pure state-machine logic (which screen, does Enter/Backspace transition correctly, does a reject bounce back to the login screen) unit-tested with injected key codes — no real window needed, same "drive it via its public API instead of a real terminal" trick `TextTestRunner` already uses. Manual keyboard-driven pass for the actual rendering once H1 is real. |
| **H4** | Networked click handling: a new controller (same `handleClick`/`handleJump` shape as `Controller`, reusing its `pixelToGrid`) that gathers two clicks client-side (first click = pending `from`; second click combines it with the last-known board token at `from` into `"WQe2e5"`) and sends the resulting string over the socket instead of calling `engine.select()`. `handleJump` sends `"JWPe2"` immediately from one click. Client-side legality (don't let the local player click the opponent's piece) is UX-only pre-filtering — the server (`GameSession::handleCommand`'s identity check, Task E2) stays the sole legality authority. | H1, H2 | Pure logic test: given a cached board + a scripted click sequence, confirm the exact wire string built, including the color-mismatch-rejected-before-sending case. Manual: a real move/jump against a live server. |
| **H5** | Wire the render loop for the **Online Play** branch only: reads the latest network-translated `GameSnapshot` (H2, refreshed via H1's socket) instead of `engine.snapshot()`; `BoardView::render()`/`HudView::compose()` calls stay byte-for-byte identical, per the "confined to HUD layer" constraint. Spectator mode disables click sending entirely (not just server-side rejection). The **Local Play** branch (H3a) is untouched — still calls `engine.snapshot()` directly, no network code anywhere on that path. | H2, H3b, H4 | Manual: two real graphics clients play a full game over the wire; visually diff against local-play behavior for anything not already covered by decision 1's degraded-fidelity call. |
| **H6** | Discrete sound/lifecycle wiring for Online Play: the network thread pushes each `sound`/`lifecycle` message onto a small mutex-guarded **queue** (not a single-slot "latest value" — every discrete event must fire, none can be dropped as stale, unlike the periodic full-state tick). The render loop drains it fully every frame and calls `SoundManager::playSound(name + ".wav")` / `hud.playEndAnimation(result)` — same effect as today's local `EventBus` subscribers, different trigger source (queue drain instead of a subscribed callback, since there's no local `EventBus`/`GameEngine` on this path). Local Play keeps its existing direct `EventBus` subscriptions, untouched. | H2, H5 | Pure logic test: a scripted sequence of queued messages dispatches the right calls in the right order. Manual: a real capture over the wire audibly plays the capture sound; a real resign/checkmate triggers the end animation. |
| **H7** | Resign UI (button or keypress sending the literal `"resign"` string, symmetric with `client/cli`'s typed command) + the disconnect countdown, via the **separate `HudView`-only side-channel struct** confirmed in decision 3 — an additive, defaulted parameter to `HudView::compose()` (e.g. `compose(boardFrame, snap, disconnectStatus = {})`) so the existing Local Play call site (H3a) doesn't need to change at all. | H5 | Manual: a real resign ends the game for both sides; a real opponent disconnect shows a live countdown on the HUD that clears on reconnect (the same three paths D4's own server-side manual verification already proved — this task only proves the client renders what the server already sends correctly). |
| **H8** | Docs: this file updated with actual task outcomes (mirrors `server-phase-plan.md`'s G1 being written last, documenting final state not intent), plus root `README.md` updated with graphics-client-online build/run instructions once real. | H1–H7 | N/A — documentation only; every command shown re-verified before being written down, matching G1's own stated bar. |

---

## Threading model (confirmed approach, not yet built)

Generalizes `client/cli`'s existing split rather than inventing a new one:

- One background thread runs `client.run()`, exactly like `client/cli`. It
  **never** calls anything in `Img`/`cv::` directly — generalizes the CLI's
  existing "network thread never touches `std::cout`" rule to "network
  thread never touches OpenCV highgui," which is a hard platform
  constraint here, not just a style choice.
- **Periodic full-state broadcasts** (H2's translated `GameSnapshot`):
  overwrite a single mutex-guarded "latest snapshot" slot (or an atomic
  `shared_ptr` swap) — the render loop only ever wants the freshest tick;
  coalescing is correct here, unlike `client/cli`'s `outputQueue` where
  every line must be shown.
- **Discrete sound/lifecycle pushes** (H6): a real queue, drained fully
  every frame — every entry must fire exactly once.
- **Login/menu/room request-response** (H3b): the same `LoginState`-shaped
  mutex+condvar `client/cli` already has, but the render loop must **poll
  it non-blockingly every frame** instead of `cv.wait()`-blocking — the
  OpenCV window has to keep pumping `show_frame`/`waitKey` during a login
  round-trip or Windows will mark it "Not Responding," and there'd be
  nowhere to show a "connecting…" status.
- Shutdown (ESC) and an unexpected server-side close both route to
  `client.stop()` + `thread.join()` (mirrors `client/cli`'s `shutdown()`
  lambda), landing on an in-window "connection lost" screen instead of
  exiting the process outright.

---

## Resuming this plan

Next step when picked back up: **H2** (the JSON→`GameSnapshot` translator)
or **H3b** (the real Online Play login/menu/room screens, replacing H3a's
"coming soon" stub) — both only depend on H1, which is done. Before either,
do the real manual click-through H3a's own verification couldn't complete
(click "Local Play" for real, confirm the board actually appears and a move
works) — cheap, and closes the one open gap in H3a's verification.

H1 and H3a are both done — the MSVC risk that gated everything else in this
plan is resolved, and local play is now cleanly separated from where the
online path will land, so H2/H4/H5/H6/H7 are no longer blocked on an open
risk, just on the remaining task order.
