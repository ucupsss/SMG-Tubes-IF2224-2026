CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -pedantic

SRC_DIR := src
INC_DIR := include
BIN_DIR := bin
TARGET := $(BIN_DIR)/lexer_app.exe
TARGET_WIN := $(subst /,\,$(TARGET))
SRC := $(SRC_DIR)/main.cpp $(SRC_DIR)/lexer.cpp $(SRC_DIR)/parser.cpp $(SRC_DIR)/formatter.cpp $(SRC_DIR)/filehandler.cpp
HEADERS := $(INC_DIR)/lexer.hpp $(INC_DIR)/parser.hpp $(INC_DIR)/formatter.hpp $(INC_DIR)/filehandler.hpp

.PHONY: all build run clean

all: build

build: $(TARGET)

$(TARGET): $(SRC) $(HEADERS)
	@echo Mengompilasi program...
	@cmd /C if not exist "$(BIN_DIR)" mkdir "$(BIN_DIR)"
	@$(CXX) $(CXXFLAGS) -I$(INC_DIR) $(SRC) -o $(TARGET)

run: $(TARGET)
	@echo Menjalankan program...
	@./$(TARGET)

clean:
	@echo Membersihkan program...
	@cmd /C if exist "$(TARGET_WIN)" del /Q "$(TARGET_WIN)"
