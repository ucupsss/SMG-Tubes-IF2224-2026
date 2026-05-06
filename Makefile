# Makefile for Arion Compiler

CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -pedantic -MMD -MP
CPPFLAGS := -Iinclude

SRC_DIR := src

ifeq ($(OS),Windows_NT)
  PLATFORM := windows
  EXE_EXT := .exe
  PATH_SEP := \\
  MKDIR = if not exist "$(subst /,\,$1)" mkdir "$(subst /,\,$1)"
  RMDIR = if exist "$(subst /,\,$1)" rmdir /s /q "$(subst /,\,$1)"
  RMFILE = if exist "$(subst /,\,$1)" del /f /q "$(subst /,\,$1)"
  RUN_PREFIX :=
else
  PLATFORM := linux
  EXE_EXT :=
  PATH_SEP := /
  MKDIR = mkdir -p "$1"
  RMDIR = rm -rf "$1"
  RMFILE = rm -f "$1"
  RUN_PREFIX := ./
endif

OBJ_DIR := build/$(PLATFORM)
BIN_DIR := bin/$(PLATFORM)
TARGET := $(BIN_DIR)/arion_compiler$(EXE_EXT)

SRCS := $(wildcard $(SRC_DIR)/*.cpp)
OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SRCS))
DEPS := $(OBJS:.o=.d)

.PHONY: all build run clean rebuild directories

all: build

build: directories $(TARGET)

directories:
	@$(call MKDIR,$(OBJ_DIR))
	@$(call MKDIR,$(BIN_DIR))

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $^ -o $@
	@echo "Build successful! Executable is at $(TARGET)"

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@$(call MKDIR,$(dir $@))
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $< -o $@

run: build
	$(RUN_PREFIX)$(TARGET)

clean:
	@$(call RMDIR,$(OBJ_DIR))
	@$(call RMFILE,$(TARGET))
	@echo "Cleaned up $(OBJ_DIR) and $(TARGET)"

clean-all:
	@$(call RMDIR,build)
	@$(call RMDIR,bin/windows)
	@$(call RMDIR,bin/linux)
	@echo "Cleaned up all build artifacts"

rebuild: clean build

-include $(DEPS)
