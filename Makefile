TARGET = kungfu_chess

# הגדרות כלליות
CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++17 --coverage
TARGET = run_tests.exe

# איסוף קבצים
# מוצא את כל קבצי ה-cpp בתיקיות src ו-tests
SOURCES = $(wildcard src/*/*.cpp) $(wildcard tests/*.cpp)

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