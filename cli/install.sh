#!/usr/bin/env bash
# lpcl-cli 一键安装脚本
# 用法: curl -fsSL <下载地址>/install.sh | bash
# 安装内容:
#   ~/.local/lib/lpcl/   — lpcl-cli 主程序 + liblpclcore.so
#   ~/.local/bin/lpcl-cli — 指向主程序的符号链接（rpath=$ORIGIN 自动找到库）
set -euo pipefail

# ---- 可配置项 ----
# 发布包下载地址（GitHub Releases 或自建服务器；也可用环境变量覆盖）
LPCL_RELEASE_URL="${LPCL_RELEASE_URL:-https://github.com/OWNER/LPCL/releases/latest/download}"
INSTALL_LIB="${LPCL_INSTALL_LIB:-$HOME/.local/lib/lpcl}"
INSTALL_BIN="${LPCL_INSTALL_BIN:-$HOME/.local/bin}"

# ---- 架构检测 ----
case "$(uname -m)" in
    x86_64)        ARCH="x86_64" ;;
    aarch64|arm64) ARCH="aarch64" ;;
    *) echo "不支持的架构: $(uname -m)" >&2; exit 1 ;;
esac

PKG="lpcl-cli-linux-${ARCH}.tar.gz"
URL="${LPCL_RELEASE_URL%/}/${PKG}"

echo "==> 下载 ${URL}"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
if ! curl -fSL --retry 3 -o "${TMP}/${PKG}" "$URL"; then
    echo "下载失败（请检查网络或 LPCL_RELEASE_URL 配置）" >&2
    exit 1
fi

echo "==> 安装到 ${INSTALL_LIB}"
mkdir -p "$INSTALL_LIB" "$INSTALL_BIN"
tar -xzf "${TMP}/${PKG}" -C "$INSTALL_LIB"
chmod +x "${INSTALL_LIB}/lpcl-cli"

echo "==> 注册命令 lpcl-cli → ${INSTALL_BIN}/lpcl-cli"
ln -sf "${INSTALL_LIB}/lpcl-cli" "${INSTALL_BIN}/lpcl-cli"

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
echo "安装完成！运行 'lpcl-cli help' 开始使用。"
