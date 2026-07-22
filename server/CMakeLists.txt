cmake_minimum_required(VERSION 3.24)
# Task C1: C enabled alongside CXX specifically for third_party/sqlite/
# sqlite3.c. With only CXX enabled, CMake has no configured C compiler and
# silently drops .c sources from the build entirely (confirmed by a real
# build - sqlite3.c never appeared in the ninja steps or the final link
# line) rather than erroring, which is a much more confusing failure mode
# than the fix. sqlite3.c genuinely needs a real C compiler, not g++ in
# C++ mode - see the matching comment in the Makefile for why (implicit
# void*-to-T* conversions that are legal C but not legal C++).
project(KungFuChessServer CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# CMAKE_CURRENT_SOURCE_DIR here = kung-fu-chess/server/
set(MAIN_PROJECT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")

# Headless server, no OpenCV/graphics dependency - deliberately a separate
# CMake project from kungfu-graphics/cpp/CMakeLists.txt (which is pinned to
# MSVC because OpenCV_451's prebuilt .lib is MSVC-ABI only). This one stays
# on the MSYS2/ucrt64 toolchain: configure with
#   cmake -S server -B server/build -G Ninja
# using ucrt64's cmake/ninja/g++ (pacman: mingw-w64-ucrt-x86_64-cmake,
# -ninja). See docs/tasks/server-phase-plan.md for the full server plan.

include(FetchContent)

# FetchContent_Populate(<name>) (used below for asio/websocketpp - fetch
# source only, deliberately skip add_subdirectory, see comments at each
# call) is deprecated as of CMake 3.30 in favor of
# FetchContent_MakeAvailable, which doesn't support "fetch but never run
# the dependency's own CMakeLists.txt". CMP0169=OLD keeps the current,
# still-functional behavior instead of silently accumulating a warning
# that CMake's own message says will eventually become a hard error -
# verified working (with only a deprecation warning, not a failure) on
# CMake 4.3.3 during Task A0's CMake migration.
if(POLICY CMP0169)
    cmake_policy(SET CMP0169 OLD)
endif()

# Pinned at 1.18.2 - NOT latest. Confirmed in Task A0: the pacman-latest
# Asio (1.38.0) has fully removed the deprecated
# io_service/io_service::strand/expires_from_now API that websocketpp
# 0.8.2 (its latest tagged release) still hard-depends on in its Asio
# transport - a real compile failure across dozens of errors, not a
# guess. See third_party/README.md for the full finding.
FetchContent_Declare(asio
    GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
    GIT_TAG        asio-1-18-2
)
FetchContent_Populate(asio)
# asio has no CMakeLists.txt of its own to build - fetch source only, use
# its include/ directory directly.

FetchContent_Declare(websocketpp
    GIT_REPOSITORY https://github.com/zaphoyd/websocketpp.git
    GIT_TAG        0.8.2
)
FetchContent_Populate(websocketpp)
# Deliberately FetchContent_Populate, not FetchContent_MakeAvailable:
# websocketpp's own CMakeLists.txt looks for Boost and builds its
# examples/tests, none of which we want. Fetch source only, use its
# top-level directory as a header-only include path.

# Reused engine layers - the same GameEngine/Controller/etc sources the
# text-protocol Makefile build and the graphics CMake build both compile.
# No audio/ (no sound on the server) and no renderer/text_io/text_test_runner
# (all graphics- or text-protocol-specific, not needed here). event_bus is
# header-only (no .cpp), just needs its include dir below.
file(GLOB ENGINE_SOURCES
    "${MAIN_PROJECT_ROOT}/src/model/*.cpp"
    "${MAIN_PROJECT_ROOT}/src/movement_rules/*.cpp"
    "${MAIN_PROJECT_ROOT}/src/real_time_arbiter/*.cpp"
    "${MAIN_PROJECT_ROOT}/src/rule_engine/*.cpp"
    "${MAIN_PROJECT_ROOT}/src/controller/*.cpp"
    "${MAIN_PROJECT_ROOT}/src/game_engine/*.cpp"
)

# server/'s own pure-logic layer (parser, serializer, session logic - not
# the WS/asio glue in main.cpp). Also dual-compiled into run_tests.exe via
# the Makefile's plain src/*/*.cpp wildcard, so it gets doctest coverage;
# nothing here touches websocketpp/Asio, so that's safe. Once a
# src/server/*.cpp file *does* need websocketpp/Asio directly (the future
# WebSocketServer.cpp, Task A5), it'll need excluding from the Makefile
# side (mirroring OPENCV_ONLY_SRC) - not needed yet.
file(GLOB SERVER_LAYER_SOURCES
    "${MAIN_PROJECT_ROOT}/src/server/*.cpp"
)

# logging/ (Task A6) - shared by server/ and, later, the shell client
# (Phase F). Also dual-compiled into run_tests.exe; no websocketpp/Asio
# dependency.
file(GLOB LOGGING_SOURCES
    "${MAIN_PROJECT_ROOT}/src/logging/*.cpp"
)

# persistence/ (Task C1) - Database wrapper, dual-compiled same as
# server/'s own layer above; no websocketpp/Asio dependency either.
file(GLOB PERSISTENCE_SOURCES
    "${MAIN_PROJECT_ROOT}/src/persistence/*.cpp"
)

# Task C2: sha256.c (vendored, third_party/sha256 - see third_party/README.md).
# Unlike sqlite3.c, this is plain portable C with no implicit
# void*-conversion reliance (confirmed compiling clean under g++ in C++
# mode, -Wall -Wextra, zero warnings) - so it's meant to compile as C++
# here too, same as the Makefile build does implicitly. But now that this
# project has C enabled (for sqlite3.c, above), CMake's default per-file
# language dispatch would compile *any* .c source - including this one -
# with the C compiler instead, which would produce plain-C-linkage symbols
# with no extern "C" wrapper in sha256.h to match them against C++ callers
# (UserRepository.cpp). Force LANGUAGE CXX explicitly so both build systems
# treat this file identically.
set(SHA256_SRC "${MAIN_PROJECT_ROOT}/third_party/sha256/sha256.c")
set_source_files_properties(${SHA256_SRC} PROPERTIES LANGUAGE CXX)

add_executable(kungfu_server
    main.cpp
    ${ENGINE_SOURCES}
    ${SERVER_LAYER_SOURCES}
    ${LOGGING_SOURCES}
    ${PERSISTENCE_SOURCES}
    # sqlite3.c: vendored C amalgamation (third_party/sqlite, see
    # third_party/README.md), needed by persistence/Database.cpp. Compiled
    # as C via CMake's automatic per-file language detection (.c vs .cpp),
    # same as any other .c source added to a CXX-language target.
    "${MAIN_PROJECT_ROOT}/third_party/sqlite/sqlite3.c"
    ${SHA256_SRC}
)

target_include_directories(kungfu_server PRIVATE
    ${asio_SOURCE_DIR}/asio/include
    ${websocketpp_SOURCE_DIR}
    "${MAIN_PROJECT_ROOT}/include/model"
    "${MAIN_PROJECT_ROOT}/include/movement_rules"
    "${MAIN_PROJECT_ROOT}/include/real_time_arbiter"
    "${MAIN_PROJECT_ROOT}/include/rule_engine"
    "${MAIN_PROJECT_ROOT}/include/controller"
    "${MAIN_PROJECT_ROOT}/include/game_engine"
    "${MAIN_PROJECT_ROOT}/include/event_bus"
    "${MAIN_PROJECT_ROOT}/include/server"
    "${MAIN_PROJECT_ROOT}/include/logging"
    "${MAIN_PROJECT_ROOT}/include/persistence"
    "${MAIN_PROJECT_ROOT}/third_party/nlohmann"
    "${MAIN_PROJECT_ROOT}/third_party/sqlite"
    "${MAIN_PROJECT_ROOT}/third_party/sha256"
)

target_compile_definitions(kungfu_server PRIVATE
    ASIO_STANDALONE
    # Bypasses a stale blanket rule in websocketpp/common/thread.hpp that
    # disables its C++11 <thread> path on any MinGW target and falls back
    # to <boost/thread.hpp> (which we don't have) - a leftover from when
    # older MinGW lacked real std::thread support, not true of the current
    # MSYS2/ucrt64 toolchain. See third_party/README.md.
    _WEBSOCKETPP_CPP11_THREAD_
)

if(WIN32)
    target_link_libraries(kungfu_server PRIVATE ws2_32 wsock32)
endif()

# Task B1: client/cli's WebSocket connectivity smoke test. Built here rather
# than as its own top-level CMake project (unlike server/ vs
# kungfu-graphics/cpp/) because the reason those two are split is a real
# MSVC-vs-MinGW toolchain conflict (OpenCV's prebuilt .lib is MSVC-ABI only);
# client/cli has no such conflict - it needs the exact same MSYS2/ucrt64 +
# asio + websocketpp dependencies as kungfu_server, so a second target here
# reuses asio_SOURCE_DIR/websocketpp_SOURCE_DIR from the FetchContent calls
# above instead of fetching a second copy of both for no benefit.
add_executable(kungfu_client
    "${MAIN_PROJECT_ROOT}/client/cli/main.cpp"
    # Task B4: prints each state broadcast's board tokens. Pure/no engine
    # dependency, so it's cheap to compile standalone here rather than
    # pulling in any of ENGINE_SOURCES - kungfu_server doesn't need this
    # (it never prints a board), hence it's only on this target, not both.
    "${MAIN_PROJECT_ROOT}/src/text_io/BoardPrinter.cpp"
)

target_include_directories(kungfu_client PRIVATE
    ${asio_SOURCE_DIR}/asio/include
    ${websocketpp_SOURCE_DIR}
    # Task B3: client sends/parses join-protocol JSON now too.
    "${MAIN_PROJECT_ROOT}/third_party/nlohmann"
    # Task B4: BoardPrinter.
    "${MAIN_PROJECT_ROOT}/include/text_io"
)

target_compile_definitions(kungfu_client PRIVATE
    ASIO_STANDALONE
    _WEBSOCKETPP_CPP11_THREAD_
)

if(WIN32)
    target_link_libraries(kungfu_client PRIVATE ws2_32 wsock32)
endif()
