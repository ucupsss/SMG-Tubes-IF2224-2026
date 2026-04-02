CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -pedantic

SRC_DIR := src
BIN_DIR := bin
TARGET := $(BIN_DIR)/lexer_app.exe
TARGET_WIN := $(subst /,\,$(TARGET))
SRC := $(SRC_DIR)/main.cpp $(SRC_DIR)/lexer.cpp

.PHONY: all build run clean

all: build

build: $(TARGET)

$(TARGET): $(SRC)
	@echo Mengompilasi program...
	@mkdir -p $(BIN_DIR)
	@$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET)

run: $(TARGET)
	@echo Menjalankan program...
	@./$(TARGET)

clean:
	@echo Membersihkan program...
	@cmd /C if exist "$(TARGET_WIN)" del /Q "$(TARGET_WIN)"
