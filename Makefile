TARGET = kungfu_chess

# הגדרות כלליות
CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++17 --coverage
TARGET = run_tests.exe

# איסוף קבצים
# מוצא את כל קבצי ה-cpp בתיקיות src ו-tests
SOURCES = $(wildcard src/*.cpp) $(wildcard tests/*.cpp)

# נתיבי ה-include
INCLUDES = -Iinclude -Isrc

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $(TARGET) $(SOURCES)

# ניקוי הבלגן
clean:
	del /Q *.gcda *.gcno 2>nul
	del /Q src\*.gcda src\*.gcno 2>nul
	del /Q tests\*.gcda tests\*.gcno 2>nul
	del /Q $(TARGET) 2>nul

.PHONY: all clean