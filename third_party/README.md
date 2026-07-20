# third_party/

## Vendored and committed to git

- **`miniaudio/`** — single-header audio library, used by `src/audio/SoundManager.cpp`.

## Fetched via CMake FetchContent (server build only)

`websocketpp` and Asio are **not** vendored here (any more — see history
below for how that changed over the course of Task A0). Both are fetched
automatically by `server/CMakeLists.txt` when you configure the server
build:

```sh
cmake -S server -B server/build -G Ninja -DCMAKE_CXX_COMPILER=g++
cmake --build server/build
```

This requires network access the first time (or whenever `server/build/`
is wiped) — there's no manual download/placement step, unlike
`kungfu-graphics/cpp/OpenCV_451/`.

- **Asio, pinned at 1.18.2** — `server/CMakeLists.txt`'s `FetchContent_Declare(asio ...)`.
  **Must be 1.18.2, not latest.** The pacman-installable
  `mingw-w64-ucrt-x86_64-asio` package is 1.38.0, which has fully removed
  the deprecated `io_service`/`io_service::strand`/`expires_from_now` API
  that `websocketpp` 0.8.2 (its latest tagged release, effectively
  unmaintained since ~2018) still hard-depends on in its Asio transport
  layer. Confirmed by an actual compile failure across dozens of errors
  (`'io_service' in namespace 'websocketpp::lib::asio' does not name a
  type`, `m_strand->wrap(...)`: "base operand of '->' is not a pointer",
  etc.) when built against 1.38.0 — this isn't a precautionary pin, the
  newer version is a real, verified break. `FetchContent_Populate` is used
  (not `FetchContent_MakeAvailable`) since Asio has no `CMakeLists.txt` of
  its own to build — fetch source only, use its `include/` directory
  directly.

- **`websocketpp`, pinned at 0.8.2** — same `FetchContent_Populate`
  pattern, and deliberately *not* `FetchContent_MakeAvailable`: its own
  `CMakeLists.txt` looks for Boost and builds its examples/tests, none of
  which we want. Also needs `-D_WEBSOCKETPP_CPP11_THREAD_` (see
  `server/CMakeLists.txt`) to bypass a stale blanket rule in
  `websocketpp/common/thread.hpp` that disables its C++11 `<thread>` path
  on any MinGW target and falls back to `<boost/thread.hpp>` (which we
  don't have) — a leftover from when older MinGW lacked real
  `std::thread` support, not true of the current MSYS2/ucrt64 toolchain.

`server/CMakeLists.txt` pins `CMP0169=OLD` to keep using
`FetchContent_Populate` without a deprecation warning — verified working
(as of CMake 4.3.3) but CMake's own message says the old-style call will
eventually be removed entirely; if that day comes, whatever replaces it
needs to preserve the same "fetch source, don't add_subdirectory"
behavior for exactly the Boost/tests reason above.

**`nlohmann::json` and the sqlite amalgamation are a different case, and
will land in the "vendored and committed" section above, not here**, when
the tasks that need them start (`GameStateSerializer`/A4, `persistence/`/C1)
— see `docs/tasks/server-phase-plan.md`. Reason: those are *dual-compiled*
pure-logic pieces that need doctest coverage under the existing
Makefile/`run_tests.exe` flow, which has no `FetchContent` equivalent —
`FetchContent` only populates content inside `server/build/`, invisible
to the Makefile build. `websocketpp`/Asio avoid that problem because
they're only ever touched by `server/main.cpp`'s accept loop, which is
CMake-only and never dual-compiled.

## History

Task A0 originally vendored both libraries by hand under `third_party/`
(`websocketpp/` committed like `miniaudio/`; `asio/` gitignored like
`OpenCV_451/`, since at ~5.7MB it read as "heavy"). Once the instructor's
build-system guidance called for CMake specifically for the server, both
were migrated to `FetchContent` instead — a cleaner single mechanism for
both dependencies, with the same pinned versions and the same
`_WEBSOCKETPP_CPP11_THREAD_` fix carried forward unchanged. The
manually-vendored copies are gone from both git and disk; nothing here
still reads either sourcing path.
