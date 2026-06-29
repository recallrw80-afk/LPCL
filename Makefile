# Quick build & run for LPCL
#   make        — build
#   make run    — build & run
#   make clean  — full clean

BUILD_DIR  := cmake-build-debug
QT_PREFIX  := $${HOME}/Qt/6.8.3/gcc_64
NPROC      := $(shell nproc)
TARGET     := ./$(BUILD_DIR)/LiunxPlainCraftLauncher

.PHONY: all run clean

all:
	cmake -B $(BUILD_DIR) -G Ninja -DCMAKE_PREFIX_PATH=$(QT_PREFIX) -DCMAKE_BUILD_TYPE=Debug
	cmake --build $(BUILD_DIR) --target LiunxPlainCraftLauncher

run: all
	$(TARGET)

clean:
	rm -rf $(BUILD_DIR)
