# Quick build & run for LPCL

BUILD_DIR  := cmake-build-debug
QT_PREFIX  ?= $${HOME}/Qt/6.11.1/gcc_64
NPROC      := $(shell nproc)

# CLI 产物全部在 cli/ 下（liblpclcore 在 LPCL 根目录，二者解耦）
CLI_SRC       := cli
CLI_BUILD_DIR := cli/cmake-build-debug
CLI_PKG_DIR   := cli/dist
CLI_PKG_BUILD := cli/cmake-build-release

.PHONY: run cli package package-cli package-tar install

# ---- QML GUI App（测试版） ----

run:
	rm -rf $(BUILD_DIR)
	cmake -B $(BUILD_DIR) -G Ninja -DCMAKE_PREFIX_PATH=$(QT_PREFIX) -DCMAKE_BUILD_TYPE=Debug
	cmake --build $(BUILD_DIR) --target lpcl-gui
	./$(BUILD_DIR)/lpcl-gui

# ---- CLI SDK ----

cli:
	rm -rf $(CLI_BUILD_DIR)
	rm -rf $(CLI_PKG_BUILD)
	cmake -B $(CLI_BUILD_DIR) -S . -G Ninja -DCMAKE_PREFIX_PATH=$(QT_PREFIX) -DCMAKE_BUILD_TYPE=Debug
	cmake --build $(CLI_BUILD_DIR) --target lpclcore lpcl

# ---- 打包发布 ----

package: package-cli
	@echo "打包完成，产物在 $(CLI_PKG_DIR)/"

# 发布构建默认不嵌入 CF key；CI 配了 Secret 时传 LPCL_EMBED_CF_KEY=ON 嵌入
LPCL_EMBED_CF_KEY ?= OFF

package-cli:
	rm -rf $(CLI_PKG_BUILD)
	cmake -B $(CLI_PKG_BUILD) -S . -G Ninja -DCMAKE_PREFIX_PATH=$(QT_PREFIX) -DCMAKE_BUILD_TYPE=Release -DLPCL_EMBED_CF_KEY=$(LPCL_EMBED_CF_KEY) -DLPCL_STATIC_CORE=ON
	cmake --build $(CLI_PKG_BUILD) --target lpcl
	mkdir -p $(CLI_PKG_DIR)
	cp $(CLI_PKG_BUILD)/cli/lpcl $(CLI_PKG_DIR)/
	@echo "CLI 打包: $(CLI_PKG_DIR)/lpcl (静态链接 liblpclcore，单文件)"

# ---- 发布压缩包（零依赖：二进制 + 收编的 Qt/第三方库 + TLS 插件） ----
ARCH := $(shell uname -m | sed 's/arm64/aarch64/')
package-tar: package-cli
	bash $(CLI_SRC)/bundle-dist.sh $(CLI_PKG_DIR) $(QT_PREFIX)
	cp THIRD-PARTY-NOTICES.md $(CLI_PKG_DIR)/
	tar -czf $(CLI_PKG_DIR)/lpcl-linux-$(ARCH).tar.gz -C $(CLI_PKG_DIR) lpcl lib plugins THIRD-PARTY-NOTICES.md
	@echo "发布包: $(CLI_PKG_DIR)/lpcl-linux-$(ARCH).tar.gz"

# ---- 编译并安装到本机（~/.local/lib/lpcl + ~/.local/bin/lpcl） ----
# 其他电脑编译的场景：在那台机器 make package-tar，把 tar.gz 拷过来后
# 执行 bash cli/install.sh lpcl-linux-<arch>.tar.gz 即可，无需克隆仓库
install: package-tar
	bash $(CLI_SRC)/install.sh $(CLI_PKG_DIR)/lpcl-linux-$(ARCH).tar.gz
