#!/bin/bash
# bundle-dist.sh — 把 lpcl 运行所需的 Qt/第三方库收进 dist/，实现零依赖分发
# 用法: bundle-dist.sh <dist目录> <Qt前缀>
# 产物布局: dist/lpcl + dist/lib/*.so + dist/plugins/tls/*.so
# 只收应用私有依赖；glibc / libstdc++ / libgcc 属系统基线，一律不带
set -euo pipefail

DIST="$1"
QT_PREFIX="$2"
BIN="$DIST/lpcl"
LIBDIR="$DIST/lib"
mkdir -p "$LIBDIR" "$DIST/plugins/tls"

# 需要收集的 soname 清单（来自 ldd 闭包，剔除 glibc/gcc 运行族）
NEEDED="
libQt6Core.so.6 libQt6Network.so.6
libicui18n.so. libicuuc.so. libicudata.so.
libz.so.1 libzstd.so.1
libgssapi_krb5.so.2 libkrb5.so.3 libk5crypto.so.3 libcom_err.so.2 libkrb5support.so.0 libkeyutils.so.1
libbrotlidec.so.1 libbrotlicommon.so.1
libglib-2.0.so.0 libgthread-2.0.so.0
libpcre2-8.so.0
"

copy_soname() {  # $1=soname(可带尾部点前缀匹配)  $2=来源 ldd 文本
    local name="$1" ldd_text="$2" path
    path=$(awk 'index($1, n) == 1 {print $3; exit}' n="$name" <<< "$ldd_text")
    if [ -z "$path" ] || [ ! -e "$path" ]; then
        echo "bundle: 找不到 $name" >&2
        return 1
    fi
    local dst="$LIBDIR/$(basename "$path")"
    # 幂等：重跑时 ldd 会解析到已收编的 lib/，源=目标则跳过
    [ "$(readlink -f "$path")" = "$(readlink -f "$dst")" ] && return 0
    cp -L "$path" "$dst"
}

BIN_LDD=$(ldd "$BIN")
for soname in $NEEDED; do
    copy_soname "$soname" "$BIN_LDD"
done

# OpenSSL：Qt6 的 TLS 插件运行时 dlopen，不出现在 ldd 里，但必须带上（HTTPS 必需）
LDC=$(ldconfig -p)
for ssl in libssl.so.3 libcrypto.so.3; do
    path=$(awk '$1 == s {print $NF; exit}' s="$ssl" <<< "$LDC")
    [ -n "$path" ] && cp -L "$path" "$LIBDIR/$ssl" || echo "bundle: 警告 未找到 $ssl（HTTPS 将不可用）" >&2
done

# TLS 插件（QNAM 走 HTTPS 必需；Qt 默认在 <程序目录>/plugins 下找）
cp -L "$QT_PREFIX/plugins/tls/libqopensslbackend.so" "$DIST/plugins/tls/"
cp -L "$QT_PREFIX/plugins/tls/libqcertonlybackend.so" "$DIST/plugins/tls/"

echo "bundle: 已收集 $(ls "$LIBDIR" | wc -l) 个库到 $LIBDIR"
