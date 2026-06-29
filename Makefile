# Quick build & run for LPCL (Qt 6.8 C++/QML port of PCL)
# Usage:
#   make          — build
#   make run      — build & run
#   make clean    — clean build dir
#   make rebuild  — clean & build
#   make debug    — rebuild from scratch (clear cmake cache)

BUILD_DIR  := cmake-build-debug
QT_PREFIX  := $${HOME}/Qt/6.8.3/gcc_64
CMAKE_ARGS := -DCMAKE_PREFIX_PATH=$(QT_PREFIX) -DCMAKE_BUILD_TYPE=Debug
TARGET     := ./$(BUILD_DIR)/LPCL

.PHONY: all run clean rebuild debug

all:
	@cmake -B $(BUILD_DIR) $(CMAKE_ARGS) 2>&1 | grep -E "Configuring|error|Error" || true
	@echo "==> Building..."
	@cmake --build $(BUILD_DIR) --target LPCL -j $$(nproc) 2>&1 | tail -5

run: all
	@echo "==> Running..."
	@$(TARGET) 2>&1

clean:
	@echo "==> Cleaning..."
	@cmake --build $(BUILD_DIR) --target clean 2>/dev/null || true
	@rm -rf $(BUILD_DIR)

rebuild: clean all

debug: clean
	@cmake -B $(BUILD_DIR) $(CMAKE_ARGS)
	@cmake --build $(BUILD_DIR) --target LPCL -j $$(nproc) 2>&1
	@echo "==> Done. Run with: $(TARGET)"
