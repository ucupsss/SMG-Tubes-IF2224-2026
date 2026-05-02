# Makefile for Arion Compiler

CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -pedantic -MMD -MP
CPPFLAGS := -Iinclude

SRC_DIR := src
OBJ_DIR := build
BIN_DIR := bin

TARGET := $(BIN_DIR)/arion_compiler

SRCS := $(shell find $(SRC_DIR) -name '*.cpp')
OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SRCS))
DEPS := $(OBJS:.o=.d)

.PHONY: all build run clean rebuild directories

all: build

build: directories $(TARGET)

directories:
	@mkdir -p $(OBJ_DIR) $(BIN_DIR)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $^ -o $@
	@echo "Build successful! Executable is at $(TARGET)"

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $< -o $@

run: build
	./$(TARGET) 

clean:
	rm -rf $(OBJ_DIR) $(TARGET)
	@echo "Cleaned up $(OBJ_DIR) and $(TARGET) \n"

rebuild: clean build

-include $(DEPS)
