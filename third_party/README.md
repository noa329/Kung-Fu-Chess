# third_party/

## Vendored and committed to git

- **`miniaudio/`** — single-header audio library, used by `src/audio/SoundManager.cpp`.
- **`websocketpp/`** — header-only WebSocket server/client library (0.8.2),
  used by the server (see `docs/tasks/server-phase-plan.md`). Small enough
  (~1MB of headers) to commit directly, same reasoning as `miniaudio/`.

## Not committed — local manual setup required

- **`asio/`** — standalone (non-Boost) Asio, **pinned at 1.18.2**. Gitignored
  like `kungfu-graphics/cpp/OpenCV_451/` (a "heavy" third-party dependency,
  ~5.7MB of headers).

  **Must be 1.18.2, not latest.** The version matters here, unusually: the
  pacman-installable `mingw-w64-ucrt-x86_64-asio` package is 1.38.0, which
  has fully removed the deprecated `io_service`/`io_service::strand`/
  `expires_from_now` API that `websocketpp` 0.8.2 (its latest tagged
  release, ~2018, effectively unmaintained since) still hard-depends on in
  its Asio transport layer. Confirmed by an actual compile failure across
  dozens of errors (`'io_service' in namespace 'websocketpp::lib::asio'
  does not name a type`, `m_strand->wrap(...)`: "base operand of '->' is
  not a pointer", etc.) when built against 1.38.0 — this isn't a
  precautionary pin, the newer version is a real, verified break.

  **Setup:**
  ```sh
  git clone --depth 1 --branch asio-1-18-2 https://github.com/chriskohlhoff/asio.git asio-src
  # then copy asio-src/asio/include/ -> third_party/asio/include/
  #      and asio-src/asio/COPYING, asio-src/asio/LICENSE_1_0.txt -> third_party/asio/
  ```

  Build flags needed wherever anything includes `websocketpp/server.hpp`
  (or any websocketpp/asio header): `-DASIO_STANDALONE
  -D_WEBSOCKETPP_CPP11_THREAD_ -I third_party/asio/include -I
  third_party/websocketpp`, plus `-lws2_32 -lwsock32` at link time. The
  `_WEBSOCKETPP_CPP11_THREAD_` define is a second, unrelated compatibility
  fix: `websocketpp/common/thread.hpp` unconditionally falls back to
  `<boost/thread.hpp>` (which we don't have) on any MinGW target, a
  leftover from when older MinGW lacked real `std::thread` support — not
  true of the current MSYS2/ucrt64 toolchain, but the check doesn't know
  that, so it has to be forced.

  Verified working: a minimal echo `websocketpp::server<asio>` compiled
  clean under g++/MSYS2 with these flags against this exact pinned pair,
  and round-tripped a real WebSocket handshake + text frame against
  `scripts/ws_test_client.py`.
