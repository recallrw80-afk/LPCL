#!/usr/bin/env bash
# lpcl 安装脚本
# 用法:
#   bash install.sh                                  # 【默认】下载官方预编译包（GitHub Releases，内嵌 CF key，完整体验）
#   bash install.sh --cn                             # 国内源：Gitee 镜像（--cn --beta 取预发布）
#   bash install.sh --beta                           # 安装最新预发布版（pre-release，releases/latest 看不到的）
#   bash install.sh ./lpcl-linux-x86_64.tar.xz       # 安装本地包（自己 make package-tar 的产物——不含内嵌 key）
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
LPCL_REPO="${LPCL_REPO:-recallrw80-afk/LPCL}"
INSTALL_LIB="${LPCL_INSTALL_LIB:-$HOME/.local/lib/lpcl}"
INSTALL_BIN="${LPCL_INSTALL_BIN:-$HOME/.local/bin}"

# ---- 平台检测 ----
OS_NAME="$(uname -s)"
if [ "$OS_NAME" = "Darwin" ]; then
    # macOS：预编译包只有 Apple Silicon（liblzma 单架构出不了 universal 包）
    case "$(uname -m)" in
        arm64)  PKG_NAME="lpcl-macos-arm64.tar.xz" ;;
        *)      echo "macOS 预编译包目前仅支持 Apple Silicon（M 系列）；Intel Mac 请走源码编译" >&2; exit 1 ;;
    esac
else
    case "$(uname -m)" in
        x86_64|amd64)    ARCH="x86_64" ;;
        aarch64|arm64)   ARCH="aarch64" ;;
        *) echo "不支持的架构: $(uname -m)" >&2; exit 1 ;;
    esac
    PKG_NAME="lpcl-linux-${ARCH}.tar.xz"
fi

# ---- 获取安装包：本地文件优先，否则下载 ----
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

# 参数解析：--cn（Gitee 国内源）/ --beta（预发布）可组合，其余第一个参数视为本地包
CN=0
BETA=0
LOCAL_FILE=""
for a in "$@"; do
    case "$a" in
        --cn) CN=1 ;;
        --beta|--pre) BETA=1 ;;
        *) [ -z "$LOCAL_FILE" ] && LOCAL_FILE="$a" ;;
    esac
done

if [ -n "$LOCAL_FILE" ]; then
    PKG_PATH="$LOCAL_FILE"
    if [ ! -f "$PKG_PATH" ]; then
        echo "安装包不存在: $PKG_PATH" >&2
        exit 1
    fi
    echo "==> 使用本地安装包 $PKG_PATH"
elif [ "$CN" = 1 ]; then
    # 国内源（Gitee）：latest 接口/列表接口 → tag，再拼下载地址
    PKG="$PKG_NAME"
    GITEE_REPO="${LPCL_GITEE_REPO:-Recall_m_wxd/lpcl}"
    if [ "$BETA" = 1 ]; then
        API="https://gitee.com/api/v5/repos/${GITEE_REPO}/releases?per_page=1"
    else
        API="https://gitee.com/api/v5/repos/${GITEE_REPO}/releases/latest"
    fi
    echo "==> 查询 Gitee 最新版本"
    TAG="$(curl -fsSL "$API" | grep -oE '"tag_name": *"[^"]+"' | head -1 | cut -d'"' -f4 || true)"
    if [ -z "$TAG" ]; then
        echo "查询失败（Gitee 还没有 Release？）" >&2
        exit 1
    fi
    URL="https://gitee.com/${GITEE_REPO}/releases/download/${TAG}/${PKG}"
    PKG_PATH="${TMP}/${PKG}"
    echo "==> 下载 ${URL}"
    if ! curl -fSL --retry 3 -o "$PKG_PATH" "$URL"; then
        echo "下载失败（请检查网络）" >&2
        exit 1
    fi
elif [ "$BETA" = 1 ]; then
    # 预发布通道：releases/latest 会跳过 pre-release，必须走列表接口取最新一条（含预发布）
    PKG="$PKG_NAME"
    API="https://api.github.com/repos/${LPCL_REPO}/releases?per_page=1"
    echo "==> 查询最新预发布版本"
    URL="$(curl -fsSL "$API" | grep -oE '"browser_download_url": *"[^"]*/'"${PKG}"'"' | head -1 | cut -d'"' -f4 || true)"
    if [ -z "$URL" ]; then
        echo "未找到包含 ${PKG} 的预发布版本" >&2
        exit 1
    fi
    PKG_PATH="${TMP}/${PKG}"
    echo "==> 下载 ${URL}"
    if ! curl -fSL --retry 3 -o "$PKG_PATH" "$URL"; then
        echo "下载失败（请检查网络；或到 Releases 页面手动下载后用本地包模式安装）" >&2
        exit 1
    fi
else
    PKG="$PKG_NAME"
    URL="${LPCL_RELEASE_URL%/}/${PKG}"
    PKG_PATH="${TMP}/${PKG}"
    echo "==> 下载 ${URL}"
    if ! curl -fSL --retry 3 -o "$PKG_PATH" "$URL"; then
        echo "下载失败（请检查网络；或到 Releases 页面手动下载后用本地包模式安装）" >&2
        echo "提示：如果目前只有预发布版本，请用 --beta 安装；国内网络可用 --cn 走 Gitee" >&2
        exit 1
    fi
fi

echo "==> 安装到 ${INSTALL_LIB}"
mkdir -p "$INSTALL_LIB" "$INSTALL_BIN"
# 清掉旧版库目录与改名前遗留，防止残留过期文件（保留 LPCL.ini 等配置）
rm -rf "${INSTALL_LIB}/lib" "${INSTALL_LIB}/plugins"
rm -f "${INSTALL_LIB}/lpcl" "${INSTALL_LIB}/lpcl-cli" "${INSTALL_LIB}/lpcl-gui" "${INSTALL_LIB}/liblpclcore.so"
# GNU/bsd tar 均自动识别 gz/xz，无需 -z/-J 参数（兼容旧 .tar.gz 本地包）
tar -xf "$PKG_PATH" -C "$INSTALL_LIB"
chmod +x "${INSTALL_LIB}/lpcl"

# macOS：清掉下载带来的 quarantine 属性，否则 Gatekeeper 会拦首次运行
if [ "$OS_NAME" = "Darwin" ]; then
    xattr -dr com.apple.quarantine "$INSTALL_LIB" 2>/dev/null || true
fi

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
