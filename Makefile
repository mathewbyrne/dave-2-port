BUILD_DIR := build
UNAME_S := $(shell uname -s)
UNAME_M := $(shell uname -m)

CXX ?= clang++
CXXFLAGS := -std=c++17 -g -O0 -fno-omit-frame-pointer -Wall -Wextra -Wno-unused-parameter

GAME_BIN := $(BUILD_DIR)/dave2
UNPACK_BIN := $(BUILD_DIR)/unpack

ifeq ($(UNAME_S),Darwin)
GAME_SRC := src/platform_macos.mm
GAME_LIBS := -framework Cocoa -framework CoreGraphics
GAME_BIN := $(BUILD_DIR)/dave2-mac
ifeq ($(UNAME_M),arm64)
GAME_ARCH := -arch arm64
endif
else ifeq ($(UNAME_S),Linux)
GAME_SRC := src/platform_linux.cpp
GAME_LIBS :=
GAME_ARCH :=
else
$(error Unsupported platform "$(UNAME_S)")
endif

.PHONY: all game unpack clean

all: game

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

game: $(BUILD_DIR)
ifeq ($(UNAME_S),Linux)
	@if [ ! -f "$(GAME_SRC)" ]; then \
		echo "Missing $(GAME_SRC). Linux platform layer is not implemented yet."; \
		exit 1; \
	fi
endif
	$(CXX) $(CXXFLAGS) $(GAME_ARCH) $(GAME_SRC) -o $(GAME_BIN) $(GAME_LIBS)

unpack: $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) src/unpack.cpp -o $(UNPACK_BIN)

clean:
	rm -rf $(BUILD_DIR)
