cmake_minimum_required(VERSION 3.16)
project(KungFuChess)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# CMAKE_CURRENT_SOURCE_DIR here = kung-fu-chess/kungfu-graphics/cpp/
set(MAIN_PROJECT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")

# docs/tasks/graphics-networked-client-plan.md, Task H1: same websocketpp/
# Asio dependency server/CMakeLists.txt already validated, fetched here too
# so the graphics binary (built under MSVC, unlike server/'s MinGW/g++) can
# link against it. Pinned at the exact same versions for the exact same
# reason recorded in server/CMakeLists.txt: Asio 1.18.2 (not latest -
# 1.38.0 removed the deprecated io_service API websocketpp 0.8.2 still
# depends on) and websocketpp 0.8.2 (its latest tagged release).
include(FetchContent)

# See server/CMakeLists.txt's identical comment: FetchContent_Populate is
# deprecated as of CMake 3.30 in favor of FetchContent_MakeAvailable, which
# doesn't support "fetch source only, never run the dependency's own
# CMakeLists.txt" - CMP0169=OLD keeps that still-functional behavior
# instead of a deprecation warning escalating to a future hard error.
if(POLICY CMP0169)
    cmake_policy(SET CMP0169 OLD)
endif()

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

# Files under kungfu-graphics/cpp/src (img.cpp, img.hpp, main.cpp)
file(GLOB LOCAL_SOURCES "src/*.cpp")
file(GLOB LOCAL_HEADERS "src/*.hpp")

# Task H1: net_smoke_test.cpp has its own main() and its own (non-OpenCV)
# target below - excluded here or the glob above would pull it into
# ${PROJECT_NAME} too, colliding with main.cpp's main() at link time.
list(REMOVE_ITEM LOCAL_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/net_smoke_test.cpp")

# The graphics view (Board_view/Piece_animator/Sprite_animation) lives in
# the main project tree, not here - explicit glob needed since the one
# above doesn't reach it.
file(GLOB RENDERER_SOURCES "${MAIN_PROJECT_ROOT}/src/renderer/*.cpp")

# The real rule engine this view is wired up to - also lives in the main
# project tree. None of these files depend on OpenCV; they're compiled into
# this OpenCV-enabled executable so the graphics main() can drive real
# GameEngine/Controller logic instead of the graphics-only demo it had before.
file(GLOB ENGINE_SOURCES
    "${MAIN_PROJECT_ROOT}/src/model/*.cpp"
    "${MAIN_PROJECT_ROOT}/src/movement_rules/*.cpp"
    "${MAIN_PROJECT_ROOT}/src/real_time_arbiter/*.cpp"
    "${MAIN_PROJECT_ROOT}/src/rule_engine/*.cpp"
    "${MAIN_PROJECT_ROOT}/src/controller/*.cpp"
    "${MAIN_PROJECT_ROOT}/src/game_engine/*.cpp"
    "${MAIN_PROJECT_ROOT}/src/audio/*.cpp"
)

# BoardParser only (not BoardPrinter - this build never prints a board) -
# main.cpp uses it to load boards/standard.txt instead of a hardcoded
# starting-position literal.
set(TEXT_IO_SOURCES "${MAIN_PROJECT_ROOT}/src/text_io/BoardParser.cpp")

set(SOURCES ${LOCAL_SOURCES} ${RENDERER_SOURCES} ${ENGINE_SOURCES} ${TEXT_IO_SOURCES})
set(HEADERS ${LOCAL_HEADERS})

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

set(OPENCV_DIR "${CMAKE_CURRENT_SOURCE_DIR}/OpenCV_451")
set(OPENCV_INCLUDE_DIR "${OPENCV_DIR}/include")
set(OPENCV_LIB_DIR "${OPENCV_DIR}/bin")

# src (img.hpp) + include/renderer (Board_view.hpp/Piece_animator.hpp/Sprite_animation.hpp)
# + every engine include folder the engine sources above need.
target_include_directories(${PROJECT_NAME} PRIVATE
    ${OPENCV_INCLUDE_DIR}
    src
    "${MAIN_PROJECT_ROOT}/include/renderer"
    "${MAIN_PROJECT_ROOT}/include/model"
    "${MAIN_PROJECT_ROOT}/include/movement_rules"
    "${MAIN_PROJECT_ROOT}/include/real_time_arbiter"
    "${MAIN_PROJECT_ROOT}/include/rule_engine"
    "${MAIN_PROJECT_ROOT}/include/controller"
    "${MAIN_PROJECT_ROOT}/include/game_engine"
    "${MAIN_PROJECT_ROOT}/include/audio"
    "${MAIN_PROJECT_ROOT}/include/event_bus"
    "${MAIN_PROJECT_ROOT}/include/text_io"
    "${MAIN_PROJECT_ROOT}/third_party/miniaudio"
)

target_link_directories(${PROJECT_NAME} PRIVATE ${OPENCV_LIB_DIR})
target_link_libraries(${PROJECT_NAME}
    $<$<CONFIG:Debug>:${OPENCV_LIB_DIR}/opencv_world451d.lib>
    $<$<CONFIG:Release>:${OPENCV_LIB_DIR}/opencv_world451.lib>
    $<$<CONFIG:RelWithDebInfo>:${OPENCV_LIB_DIR}/opencv_world451.lib>
    $<$<CONFIG:MinSizeRel>:${OPENCV_LIB_DIR}/opencv_world451.lib>
)

# miniaudio (WASAPI backend) calls into COM (CoInitializeEx/PropVariantClear)
# directly rather than dynamically loading it like most other Windows APIs it
# uses, so it needs to be linked against explicitly.
if(WIN32)
    target_link_libraries(${PROJECT_NAME} ole32)
endif()

if(WIN32)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<$<CONFIG:Debug>:${OPENCV_LIB_DIR}/opencv_world451d.dll>
        $<$<CONFIG:Release>:${OPENCV_LIB_DIR}/opencv_world451.dll>
        $<$<CONFIG:RelWithDebInfo>:${OPENCV_LIB_DIR}/opencv_world451.dll>
        $<$<CONFIG:MinSizeRel>:${OPENCV_LIB_DIR}/opencv_world451.dll>
        $<TARGET_FILE_DIR:${PROJECT_NAME}>)
endif()

# Absolute path to kungfu-graphics/ and to the repo-root assets/sounds/
# folder, as #defines, so main.cpp doesn't have to guess the working
# directory it's launched from (see main.cpp).
target_compile_definitions(${PROJECT_NAME} PRIVATE
    KUNGFU_ASSETS_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/../"
    KUNGFU_SOUNDS_ROOT="${MAIN_PROJECT_ROOT}/assets/sounds"
)

message(STATUS "Found source files: ${SOURCES}")
message(STATUS "Found header files: ${HEADERS}")

# docs/tasks/graphics-networked-client-plan.md, Task H1: proves websocketpp/
# Asio/nlohmann actually compile and link under this project's MSVC
# toolchain (server/ only ever proved this under MinGW/g++ - a real,
# previously-unverified risk, see the plan). Deliberately a separate,
# OpenCV-free target from ${PROJECT_NAME} - isolates this proof so it can
# fail/iterate without risking the existing, working local-play build, and
# needs none of KungFuChess's own dependencies to do its job (connect to a
# server and print a line). Superseded by the real include/net_client
# layer once Task H2 lands - not meant to be the permanent home for this
# code.
add_executable(KungFuChessNetSmokeTest src/net_smoke_test.cpp)

target_include_directories(KungFuChessNetSmokeTest PRIVATE
    ${asio_SOURCE_DIR}/asio/include
    ${websocketpp_SOURCE_DIR}
    "${MAIN_PROJECT_ROOT}/third_party/nlohmann"
)

target_compile_definitions(KungFuChessNetSmokeTest PRIVATE
    ASIO_STANDALONE
    # Unlike server/CMakeLists.txt, no _WEBSOCKETPP_CPP11_THREAD_ define
    # here: that bypasses a blanket rule in websocketpp/common/thread.hpp
    # that only triggers when __MINGW32__/__MINGW64__ is defined, which is
    # never true under MSVC - so it would be a no-op here, not a "matches
    # server/ for consistency" case worth keeping.
    #
    # _WEBSOCKETPP_CPP11_STL_ IS needed here, though, and is the first real
    # MSVC-specific finding from Task H1: MSVC reports __cplusplus as the
    # pre-C++11 value 199711L by default regardless of /std:c++17(a
    # documented MSVC quirk - the real value needs an opt-in /Zc:__cplusplus
    # flag MSVC doesn't set by default), so websocketpp/common/cpp11.hpp's
    # own __cplusplus >= 201103L feature-detection never fires and
    # common/random.hpp falls through to a `#include <boost/version.hpp>`
    # path we don't have - confirmed by a real C1083 "Cannot open include
    # file: 'boost/version.hpp'" build failure, not a guess. websocketpp's
    # own cpp11.hpp comment documents this exact define as the intended
    # escape hatch for a build system that knows better than __cplusplus.
    _WEBSOCKETPP_CPP11_STL_
)

if(WIN32)
    target_link_libraries(KungFuChessNetSmokeTest PRIVATE ws2_32 wsock32)
endif()
