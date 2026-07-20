# Quick build & run for LPCL

BUILD_DIR  := cmake-build-debug
QT_PREFIX  := $${HOME}/Qt/6.11.1/gcc_64
NPROC      := $(shell nproc)

# CLI 产物全部在 cli/ 下（liblpclcore 在 LPCL 根目录，二者解耦）
CLI_SRC       := cli
CLI_BUILD_DIR := cli/cmake-build-debug
CLI_PKG_DIR   := cli/dist
CLI_PKG_BUILD := cli/cmake-build-release

.PHONY: run cli package package-cli package-tar

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
	cmake -B $(CLI_BUILD_DIR) -S . -G Ninja -DCMAKE_PREFIX_PATH=$(QT_PREFIX) -DCMAKE_BUILD_TYPE=Debug
	cmake --build $(CLI_BUILD_DIR) --target lpclcore lpcl-cli

# ---- 打包发布 ----

package: package-cli
	@echo "打包完成，产物在 $(CLI_PKG_DIR)/"

package-cli:
	rm -rf $(CLI_PKG_BUILD)
	cmake -B $(CLI_PKG_BUILD) -S . -G Ninja -DCMAKE_PREFIX_PATH=$(QT_PREFIX) -DCMAKE_BUILD_TYPE=Release -DLPCL_EMBED_CF_KEY=OFF
	cmake --build $(CLI_PKG_BUILD) --target lpclcore lpcl-cli
	mkdir -p $(CLI_PKG_DIR)
	cp $(CLI_PKG_BUILD)/cli/lpcl-cli $(CLI_PKG_DIR)/
	cp $(CLI_PKG_BUILD)/sdk/liblpclcore.so $(CLI_PKG_DIR)/
	@echo "CLI 打包: $(CLI_PKG_DIR)/"
	@echo "  lpcl-cli         (rpath=\$$ORIGIN)"
	@echo "  liblpclcore.so"

# ---- 发布压缩包（配合 cli/install.sh 的 curl|bash 安装） ----
ARCH := $(shell uname -m | sed 's/arm64/aarch64/')
package-tar: package-cli
	tar -czf $(CLI_PKG_DIR)/lpcl-cli-linux-$(ARCH).tar.gz -C $(CLI_PKG_DIR) lpcl-cli liblpclcore.so
	@echo "发布包: $(CLI_PKG_DIR)/lpcl-cli-linux-$(ARCH).tar.gz"
