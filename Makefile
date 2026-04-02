CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -pedantic
TARGET = lexer_app
SRC = src/main.cpp src/lexer.cpp

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(SRC)
	@echo Mengompilasi program...
	@$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET)

run: $(TARGET)
	@echo Menjalankan program...
	@./$(TARGET)

clean:
	@echo Membersihkan program...
	@del /Q $(TARGET).exe 2>nul || rm -f $(TARGET)
