cmake_minimum_required(VERSION 3.24)
project(KungFuChessServer CXX)

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

add_executable(kungfu_server
    main.cpp
    ${ENGINE_SOURCES}
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
