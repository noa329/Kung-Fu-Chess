TARGET = kungfu_chess

CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++17

SOURCES = main.cpp Game.cpp

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SOURCES)

clean:
	rm -f $(TARGET)