CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -pedantic
TARGET = lexer_app
SRC = src/main.cpp src/lexer.cpp

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET)

run: $(TARGET)
	./$(TARGET)

clean:
	del /Q $(TARGET).exe 2>nul || rm -f $(TARGET)
