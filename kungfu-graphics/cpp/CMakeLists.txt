cmake_minimum_required(VERSION 3.16)
project(KungFuChess)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# CMAKE_CURRENT_SOURCE_DIR here = kung-fu-chess/kungfu-graphics/cpp/
set(MAIN_PROJECT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")

# Files under kungfu-graphics/cpp/src (img.cpp, img.hpp, main.cpp)
file(GLOB LOCAL_SOURCES "src/*.cpp")
file(GLOB LOCAL_HEADERS "src/*.hpp")

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

set(SOURCES ${LOCAL_SOURCES} ${RENDERER_SOURCES} ${ENGINE_SOURCES})
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
