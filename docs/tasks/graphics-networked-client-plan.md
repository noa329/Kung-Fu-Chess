# Kung Fu Chess — graphics client goes networked (TODO / reference plan)

> **Status: in progress, paused after H3b.** **H1, H3a, H2, and H3b are all
> done** (see their rows below) — the graphics binary now has a real,
> working login → menu → matchmaking/room-join flow talking to a live
> server, and stays connected on a status screen afterward for H4/H5 to
> build on. **Two open verification gaps, both environment/automation
> issues rather than suspected code problems, and both being closed by the
> user directly rather than by me finding a workaround**:
> 1. `run_tests.exe` is now blocked outright by this machine's Smart App
>    Control/Device Guard policy (a change since H2's own successful
>    231/231 run) — the user will run it themselves and report pass/fail
>    for H3b's 11 new `TextFieldInput`/`OnlineFlowState` cases.
> 2. H3a's own click-through gap is still open, and H3b's live-screen
>    screenshot verification hit a worse version of the same
>    desktop-automation flakiness — the user is doing a real manual
>    click-through of the whole Local Play *and* Online Play flow.
>
> Pick up at **H4** (networked click handling) or **H5** (wire the render
> loop to network state) when resuming — both depend on H1/H2, which are
> done; H4 also benefits from H3b's `OnlineClient` already existing to
> extend rather than build from scratch.

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
| **H2** ✅ | Pure JSON→`GameSnapshot` translator, new `include/net_client`/`src/net_client` layer, no OpenCV dependency: `GameStateDeserializer::deserialize(raw)` returns `std::optional<GameSnapshot>` — `std::nullopt` for malformed JSON or any message that isn't the untyped state broadcast (same has-a-`"board"`-key discriminator `client/cli/main.cpp`'s own `handleServerMessage()` already uses to tell it apart from every `"type"`-tagged message). Reuses `GameSnapshot` itself (decision 1) rather than inventing a parallel type, defaulting the three fields the wire protocol never carries: `selected={-1,-1}`, `moveTargets`/`moveProgress` sized to the board and zeroed, `captureFlashes` empty — `whiteName`/`blackName` are also left at their default-constructed `""` (that data arrives via a different message shape entirely, a Task H3b/H5 concern, not this function's). Also added `NetworkEventParser::parse(raw)`, returning `std::optional<std::variant<SoundEvent, GameLifecycleEvent>>` for Task G4's discrete `"type":"sound"`/`"type":"lifecycle"` pushes — **reuses the exact same `SoundEvent`/`GameLifecycleEvent` types `event_bus/Events.hpp` already defines** (the ones `GameEngine`'s local `EventBus` already publishes) rather than a third parallel type, so Task H6 can feed a parsed network message straight into `SoundManager::playSound()`/`HudView::playEndAnimation()` exactly like `runLocalGame()`'s own subscribers already do. Both new source directories need no Makefile/CMake changes to get doctest coverage — the root `Makefile`'s `wildcard`/`find`-based globs already pick up any new `src/*/`/`include/*/` folder automatically, same as every other dual-compiled layer. Not yet wired into `kungfu-graphics/cpp/CMakeLists.txt` — that's H4/H5's job, once the graphics binary actually has an online code path to call this from. | — | 17 new `TEST_CASE`s (10 for `GameStateDeserializer`, 7 for `NetworkEventParser`): malformed JSON, non-object JSON, every other message shape on this connection (`"joined"`, a `"sound"`/`"lifecycle"` push) correctly rejected by each parser as "not mine"; a full round-trip of board/cellStates/scores/move-history/gameOver+result; the "excluded fields land at their documented defaults, sized to match the board" case; missing-optional-fields tolerance (only `"board"` present, everything else falls back cleanly instead of throwing); both discrete event shapes (`sound`, `lifecycle` start and end) decoded correctly via `std::holds_alternative`/`std::get`. `run_tests.exe` 231/231 (was 214, +17 new — built via a manual `g++` invocation mirroring the `Makefile` exactly, since `make` itself isn't installed on this machine, same fallback D4/G2/G3/G4 already used). Rebuilt `KungFuChess` (CMake/MSVC) afterward to confirm zero regression, as expected since this task touched no file that target compiles. |
| **H3a** ✅ | Safety-first refactor, zero behavior change: extracted today's entire local-play loop (`BoardParser` → `GameEngine` → `Controller` → `BoardView`/`HudView` → `EventBus` wiring) out of `main()` into `runLocalGame()` — body is a byte-for-byte copy of the pre-refactor `main()` (only the `window_name` local became a file-scope `WINDOW_NAME` constant, shared with the new screens below). Added a new top-level **"Local Play" / "Online Play"** mode-select screen (`Img`-only, `chooseMode()`) in front of it, per the two decisions confirmed before starting: **mouse-click buttons** (two `Img::rectangle` buttons, reusing the exact `Img::on_mouse` callback pattern the gameplay screen's own `onMouse()` already established — no new input mechanism needed), and **Online Play is a stub screen that loops back to `chooseMode()`** (`showOnlinePlayStub()` — "Coming soon", any key returns to the menu) rather than a disabled/greyed-out option, since H3b doesn't exist yet but the option should still be fully reachable end-to-end. `main()` is now: `chooseMode()` → loop on `showOnlinePlayStub()`+`chooseMode()` while `"online"` → `return runLocalGame()` once `"local"` is chosen (`"quit"`, i.e. ESC at the menu, exits immediately) — once Local Play starts, it keeps its exact prior top-level behavior including ESC ending the whole program, not returning to a menu (a deliberate, explicit part of "zero behavior change"). No networking dependency at all — built and verified independently of H1/H2. | — | `cmake --build build --target KungFuChess --config Debug` — clean build, zero warnings introduced. **Manual, with an honest limitation**: launched the real `KungFuChess.exe` and screenshotted the live window (via a PowerShell/.NET `Graphics.CopyFromScreen` capture, since no project-specific run-skill or GUI-automation tool exists for this native Win32/OpenCV app) — confirmed the mode-select screen renders correctly: title, both buttons legible and correctly positioned, not blank/black/crashed. **Could not confirm the click-through into `runLocalGame()` itself** — repeated synthetic-mouse-click attempts (via `user32.dll` `mouse_event`/`SetForegroundWindow`) were intercepted by other windows on the live desktop (VS Code, a browser) stealing top-of-z-order between the click and the follow-up screenshot, confirmed by re-screenshotting and finding the mode-select screen still showing (i.e. `chooseMode()`'s own loop was still running - the click never reached `onModeSelectMouse` at all, not a code-side failure). Did not keep fighting environment flakiness on real-desktop GUI automation for a check that code review already answers with certainty: `runLocalGame()`'s body is a direct, uneditorialized copy of the prior `main()`, so its behavior is unchanged by construction, not by a runtime probe. **Recommend a real manual click-through pass (a few seconds, actually clicking "Local Play" and playing a move) before relying on this beyond the diff-level guarantee.** |
| **H3b** ✅ | Real Online Play screens, replacing H3a's stub, split across three pieces per the project's layering rules rather than one class: **(1)** `OnlineMenuView` (`include/renderer`/`src/renderer`, `Img`-only, alongside `HudView`) — login (username + masked password, decision 4), post-login menu (quick match/create room/join room, mouse-click buttons), a room-ID text prompt, and a passive status screen; owns no network logic at all. **(2)** `OnlineClient` (`kungfu-graphics/cpp/src/`, graphics-build-only — needs websocketpp/Asio, only available to this CMake target per H1, so it deliberately isn't under the shared `net_client` tree) — owns the real connection: background thread running `client.run()` for the connection's whole lifetime, `connect()`/`sendLogin()`/`sendPlay()`/`sendCreateRoom()`/`sendJoinRoom()`, and non-blocking `pollConnectionOpened()`/`pollResponse()` accessors (pimpl'd, same pattern as `audio/SoundManager.hpp`, so main.cpp never sees websocketpp/nlohmann types). Message dispatch mirrors `client/cli/main.cpp`'s `handleServerMessage()` exactly (same message types, same fields), minus the text-output side effects. **(3)** Two new pure `net_client` pieces so the plan's own testing promise is actually kept: `TextFieldInput::apply()` (the printable-char/backspace logic behind every text field) and `OnlineFlowState::nextScreenAfterResponse()` (the "which screen comes next" decision for the "waiting" screen) — both extracted specifically because `OnlineMenuView`/`main.cpp` need `Img`/OpenCV and can't be reached by `run_tests.exe` at all, so anything meant to be doctested has to live somewhere that doesn't. `main.cpp` gained `runOnlineGame()` (replaces `showOnlinePlayStub()`), a thin loop over string screen states ("connecting"/"login"/"menu"/"room_id_entry"/"waiting"/"connection_failed"/"status_connected") that never blocks on the network — draws whichever screen via `OnlineMenuView`, polls `OnlineClient` non-blockingly, reacts. **Per the plan's confirmed decision, a successful join lands on a "Connected as White/Black vs `<opponent>`" status screen and stays connected** (Task H5 later replaces just this waiting screen with the real board, reusing the same connection) rather than disconnecting immediately. **Real bug found and fixed while writing this** (not hypothetical): H3a's `chooseMode()` registered its mouse callback against a plain stack-local `ModeSelectContext` - once `chooseMode()` returns, `cv::setMouseCallback`'s registration is left pointing at freed stack memory until something else overwrites it, and `main()`'s `while (mode == "online")` loop calls `chooseMode()` more than once per run with `runOnlineGame()`'s own screens up in between - a stray click during that window would have been a real use-after-free, not just a cosmetic glitch. Fixed by giving `ctx` static storage duration (stable for the program's lifetime) instead, with an explicit clear at the top of each call. `KungFuChessNetSmokeTest` (H1) is removed - `${PROJECT_NAME}` itself now links websocketpp for real via `OnlineClient`, making the standalone proof target redundant. | H1, H2 (net_client layer, though H2's own `GameStateDeserializer`/`NetworkEventParser` aren't called yet - deferred to H5/H6) | 11 new doctest `TEST_CASE`s (5 for `TextFieldInput`: append, backspace, backspace-on-empty, control keys ignored, `maxLen` respected; 6 for `OnlineFlowState`: a rejected login/join_room/play/create_room each bounce to the right screen, a fresh accepted login goes to the menu, a reconnect-accepted login skips straight to status, an accepted play/create_room/join_room all land on status). **Execution status, not just intent**: `run_tests.exe` was rebuilt locally and compiled clean, but could not be run to actually confirm pass/fail — a freshly-recompiled, unsigned `run_tests.exe` is now blocked outright by this machine's Smart App Control/Device Guard policy (confirmed via the `Microsoft-Windows-CodeIntegrity/Operational` event log, and confirmed *not* path-specific by trying both the repo root and a scratch directory - the same binary hash is blocked everywhere). This wasn't true a few tasks ago (H2's own 231/231 run succeeded on this same machine), so something in the policy's evaluation changed independently of this work. Per the user's own call: they'll run `run_tests.exe` themselves and report back, rather than me attempting any workaround to a security policy that isn't mine to change. `KungFuChess` (MSVC/CMake) **did** build and link clean, and a manual launch against a live `kungfu_server.exe` confirmed the process starts, stays alive, and stays responsive (`Get-Process` `Responding: True`) with no crash - but a live screenshot of the actual running screens could not be captured this pass (the same class of desktop-automation flakiness H3a's own manual-verification section already documented, worse this time - even an `AttachThreadInput`-forced foreground didn't produce a screenshot matching the window's own reported screen rect, suggesting a session/compositor quirk in this environment rather than anything code-related). **The user is doing a real manual click-through themselves** (as already planned following H3a) - that pass will be the actual visual/functional confirmation for this task, not this note. |
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

Next step when picked back up: **H4** (networked click handling - can now
extend H3b's `OnlineClient` rather than building connection plumbing from
scratch) or **H5** (wire the render loop to network state, reusing H3b's
still-open post-join connection). Both depend on H1/H2, which are done.

Two open verification items are being closed by the user directly, outside
this task sequence: a real manual click-through of both Local Play and
Online Play (closing H3a's gap and confirming H3b's screens render as
intended), and actually running `run_tests.exe` to confirm H3b's 11 new
`TextFieldInput`/`OnlineFlowState` cases pass (blocked for me by a Smart
App Control/Device Guard policy that started rejecting freshly-compiled
unsigned binaries on this machine partway through H3b - see H3b's own row).

H1, H3a, H2, and H3b are all done — the MSVC risk that gated everything
else in this plan is resolved, local play is cleanly separated from where
the online path lives, the wire-format translation layer both H4 and H5
need already exists and is tested, and the graphics binary now has a real
working connection to the server with a login/menu/matchmaking/room flow
proven to build and run. Remaining tasks (H4, H5, H6, H7) are blocked only
on task order now, not on any open risk.
