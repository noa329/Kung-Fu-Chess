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
SOURCES = $(filter-out $(OPENCV_ONLY_SRC),$(ALL_SRC)) $(wildcard tests/*.cpp)

# נתיבי ה-include - כל תת-תיקייה בנפרד, כי ה-#include-ים בקוד
# משתמשים בשמות קבצים בלבד (לא נתיב מלא). זה בכוונה, כדי שהקוד
# יתקמפל גם במערכות שמשטחות קבצים (כמו VPL) וגם מקומית.
INCLUDE_DIRS := $(shell find include -type d) $(shell find src -type d)
INCLUDES = $(addprefix -I,$(INCLUDE_DIRS))

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $(TARGET) $(SOURCES)

# ניקוי הבלגן
clean:
	del /Q *.gcda *.gcno 2>nul
	del /Q src\*\*.gcda src\*\*.gcno 2>nul
	del /Q tests\*.gcda tests\*.gcno 2>nul
	del /Q $(TARGET) 2>nul

.PHONY: all clean