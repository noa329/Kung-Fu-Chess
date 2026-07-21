TARGET = kungfu_chess

# הגדרות כלליות
CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++17
TARGET = run_tests.exe

# איסוף קבצים
# מוצא את כל קבצי ה-cpp בתיקיות src ו-tests, מלבד קבצי renderer שתלויים
# ב-OpenCV/img.hpp (שיושבים רק מתחת ל-kungfu-graphics/cpp/) - אלה מתקמפלים
# בנפרד דרך ה-CMake/OpenCV build, ואף בדיקה תחת tests/ לא בודקת אותם.
ALL_SRC := $(wildcard src/*/*.cpp)
OPENCV_ONLY_SRC := src/renderer/Board_view.cpp src/renderer/Piece_animator.cpp src/renderer/Sprite_animation.cpp src/renderer/Hud_view.cpp src/renderer/RestDurationLoader.cpp
# WebSocketServer.cpp needs websocketpp/Asio, which this build has no access
# to (they're CMake FetchContent-only, see server/CMakeLists.txt and
# third_party/README.md) - everything else under src/server/ is pure logic
# with no such dependency, so it stays in SOURCES and gets doctest coverage.
SERVER_ONLY_SRC := src/server/WebSocketServer.cpp
SOURCES = $(filter-out $(OPENCV_ONLY_SRC) $(SERVER_ONLY_SRC),$(ALL_SRC)) $(wildcard tests/*.cpp)

# נתיבי ה-include - כל תת-תיקייה בנפרד, כי ה-#include-ים בקוד
# משתמשים בשמות קבצים בלבד (לא נתיב מלא). זה בכוונה, כדי שהקוד
# יתקמפל גם במערכות שמשטחות קבצים (כמו VPL) וגם מקומית.
INCLUDE_DIRS := $(shell find include -type d) $(shell find src -type d) third_party/miniaudio third_party/nlohmann third_party/sqlite
INCLUDES = $(addprefix -I,$(INCLUDE_DIRS))

# miniaudio (used by src/audio/SoundManager.cpp, pulled in via GameEngine)
# calls into COM directly on its WASAPI backend, so it needs ole32 linked in
# even for this text-protocol/tests build - see kungfu-graphics/cpp/CMakeLists.txt
# for the same requirement on the graphics build.
LDLIBS = -lole32

# Task C1: sqlite3.c (vendored amalgamation, third_party/sqlite - see
# third_party/README.md) is real C, not C++ - it relies pervasively on
# implicit void*-to-T* conversions (sqlite3DbMallocRaw() etc.), which is
# legal C but a hard error under C++'s stricter conversion rules. Compiling
# it via g++ (as any other file in SOURCES would be) fails with dozens of
# "invalid conversion from 'void*'" errors - confirmed by a real build
# attempt, not a guess. Fix: compile it as its own object with a real C
# compiler (gcc, not g++) via a dedicated rule, then link that object
# alongside the C++ objects in the final g++ link step. This is the
# standard, sqlite.org-documented way to embed the amalgamation in a C++
# project - not a workaround specific to this repo.
CC = gcc
CFLAGS = -O2 -std=c11
SQLITE_OBJ := third_party/sqlite/sqlite3.o

all: $(TARGET)

$(TARGET): $(SOURCES) $(SQLITE_OBJ)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $(TARGET) $(SOURCES) $(SQLITE_OBJ) $(LDLIBS)

$(SQLITE_OBJ): third_party/sqlite/sqlite3.c
	$(CC) $(CFLAGS) -I third_party/sqlite -c third_party/sqlite/sqlite3.c -o $(SQLITE_OBJ)

# ניקוי הבלגן
clean:
	del /Q *.gcda *.gcno 2>nul
	del /Q src\*\*.gcda src\*\*.gcno 2>nul
	del /Q tests\*.gcda tests\*.gcno 2>nul
	del /Q $(SQLITE_OBJ) 2>nul
	del /Q $(TARGET) 2>nul

.PHONY: all clean