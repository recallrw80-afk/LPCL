# Quick build & run for LPCL

BUILD_DIR  := cmake-build-debug
QT_PREFIX  := $${HOME}/Qt/6.11.1/gcc_64
NPROC      := $(shell nproc)

.PHONY: run

run:
	rm -rf $(BUILD_DIR)
	cmake -B $(BUILD_DIR) -G Ninja -DCMAKE_PREFIX_PATH=$(QT_PREFIX) -DCMAKE_BUILD_TYPE=Debug
	cmake --build $(BUILD_DIR) --target LiunxPlainCraftLauncher
	./$(BUILD_DIR)/LiunxPlainCraftLauncher
