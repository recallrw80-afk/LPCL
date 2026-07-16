# Quick build & run for LPCL

BUILD_DIR  := cmake-build-debug
QT_PREFIX  := $${HOME}/Qt/6.11.1/gcc_64
NPROC      := $(shell nproc)

# CLI 产物全部在 cli/ 下
CLI_SRC       := cli
CLI_BUILD_DIR := cli/cmake-build-debug
CLI_PKG_DIR   := cli/dist
CLI_PKG_BUILD := cli/cmake-build-release

.PHONY: run cli package package-cli

# ---- QML GUI App ----

run:
	rm -rf $(BUILD_DIR)
	cmake -B $(BUILD_DIR) -G Ninja -DCMAKE_PREFIX_PATH=$(QT_PREFIX) -DCMAKE_BUILD_TYPE=Debug
	cmake --build $(BUILD_DIR) --target LiunxPlainCraftLauncher
	./$(BUILD_DIR)/LiunxPlainCraftLauncher

# ---- CLI SDK ----

cli:
	rm -rf $(CLI_BUILD_DIR)
	rm -rf $(CLI_PKG_BUILD)
	cmake -B $(CLI_BUILD_DIR) -S $(CLI_SRC) -G Ninja -DCMAKE_PREFIX_PATH=$(QT_PREFIX) -DCMAKE_BUILD_TYPE=Debug
	cmake --build $(CLI_BUILD_DIR) --target lpclcore lpcl-cli

# ---- 打包发布 ----

package: package-cli
	@echo "打包完成，产物在 $(CLI_PKG_DIR)/"

package-cli:
	rm -rf $(CLI_PKG_BUILD)
	cmake -B $(CLI_PKG_BUILD) -S $(CLI_SRC) -G Ninja -DCMAKE_PREFIX_PATH=$(QT_PREFIX) -DCMAKE_BUILD_TYPE=Release
	cmake --build $(CLI_PKG_BUILD) --target lpclcore lpcl-cli
	mkdir -p $(CLI_PKG_DIR)
	cp $(CLI_PKG_BUILD)/lpcl-cli $(CLI_PKG_DIR)/
	cp $(CLI_PKG_BUILD)/liblpclcore/liblpclcore.so $(CLI_PKG_DIR)/
	@echo "CLI 打包: $(CLI_PKG_DIR)/"
	@echo "  lpcl-cli         (rpath=\$$ORIGIN)"
	@echo "  liblpclcore.so"
