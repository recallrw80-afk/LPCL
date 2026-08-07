#!/usr/bin/env bash
# lpcl 安装脚本
# 用法:
#   bash install.sh                                  # 【默认】下载官方预编译包（CI 构建，内嵌 CF key，完整体验）
#   bash install.sh ./lpcl-linux-x86_64.tar.gz       # 安装本地包（自己 make package-tar 的产物——不含内嵌 key）
#   curl -fsSL <发布地址>/install.sh | bash           # 一键安装
#
# 安装内容:
#   ~/.local/lib/lpcl/      — lpcl 主程序 + lib/（收编的 Qt/第三方库）+ plugins/（TLS 插件）
#   ~/.local/bin/lpcl       — 指向主程序的符号链接（rpath=$ORIGIN 自动找到库）
#   ~/.local/bin/lpcl-gui   — 包内含 lpcl-gui 时注册（GUI 测试版入口）
set -euo pipefail

# ---- 可配置项 ----
# 发布包下载地址（GitHub Releases 或自建服务器；也可用环境变量覆盖）
LPCL_RELEASE_URL="${LPCL_RELEASE_URL:-https://github.com/recallrw80-afk/LPCL/releases/latest/download}"
INSTALL_LIB="${LPCL_INSTALL_LIB:-$HOME/.local/lib/lpcl}"
INSTALL_BIN="${LPCL_INSTALL_BIN:-$HOME/.local/bin}"

# ---- 架构检测 ----
case "$(uname -m)" in
    x86_64|amd64)    ARCH="x86_64" ;;
    aarch64|arm64)   ARCH="aarch64" ;;
    *) echo "不支持的架构: $(uname -m)" >&2; exit 1 ;;
esac

# ---- 获取安装包：本地文件优先，否则下载 ----
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

if [ $# -ge 1 ]; then
    PKG_PATH="$1"
    if [ ! -f "$PKG_PATH" ]; then
        echo "安装包不存在: $PKG_PATH" >&2
        exit 1
    fi
    echo "==> 使用本地安装包 $PKG_PATH"
else
    PKG="lpcl-linux-${ARCH}.tar.gz"
    URL="${LPCL_RELEASE_URL%/}/${PKG}"
    PKG_PATH="${TMP}/${PKG}"
    echo "==> 下载 ${URL}"
    if ! curl -fSL --retry 3 -o "$PKG_PATH" "$URL"; then
        echo "下载失败（请检查网络；或到 Releases 页面手动下载后用本地包模式安装）" >&2
        exit 1
    fi
fi

echo "==> 安装到 ${INSTALL_LIB}"
mkdir -p "$INSTALL_LIB" "$INSTALL_BIN"
# 清掉旧版库目录与改名前遗留，防止残留过期文件（保留 LPCL.ini 等配置）
rm -rf "${INSTALL_LIB}/lib" "${INSTALL_LIB}/plugins"
rm -f "${INSTALL_LIB}/lpcl" "${INSTALL_LIB}/lpcl-cli" "${INSTALL_LIB}/lpcl-gui" "${INSTALL_LIB}/liblpclcore.so"
tar -xzf "$PKG_PATH" -C "$INSTALL_LIB"
chmod +x "${INSTALL_LIB}/lpcl"

echo "==> 注册命令 lpcl → ${INSTALL_BIN}/lpcl"
ln -sf "${INSTALL_LIB}/lpcl" "${INSTALL_BIN}/lpcl"
rm -f "${INSTALL_BIN}/lpcl-cli"   # 改名前遗留链接

# 包内含 GUI 时注册 lpcl-gui
if [ -x "${INSTALL_LIB}/lpcl-gui" ]; then
    echo "==> 注册命令 lpcl-gui → ${INSTALL_BIN}/lpcl-gui"
    ln -sf "${INSTALL_LIB}/lpcl-gui" "${INSTALL_BIN}/lpcl-gui"
fi

# ---- 验证 ----
if "${INSTALL_LIB}/lpcl" version >/dev/null 2>&1; then
    echo "==> 已安装: $("${INSTALL_LIB}/lpcl" version | head -1)"
else
    echo "警告: lpcl 无法运行（架构不匹配？），请检查安装包" >&2
fi

# ---- PATH 检查 ----
case ":${PATH}:" in
    *":${INSTALL_BIN}:"*) ;;
    *)
        echo ""
        echo "注意: ${INSTALL_BIN} 不在 PATH 中。请把下面这行加入你的 shell 配置（~/.bashrc 或 ~/.zshrc）："
        echo ""
        echo "    export PATH=\"${INSTALL_BIN}:\$PATH\""
        echo ""
        ;;
esac

echo ""
echo "安装完成！运行 'lpcl help' 开始使用。"
