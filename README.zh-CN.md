# LPCL — Linux Plain Craft Launcher

[English](README.md) | **简体中文**

一个用 C++20 / Qt 6 编写的跨平台 Minecraft 启动器，以轻量命令行前端 `lpcl`（RSS ~4MB）为核心，适合桌面、服务器与无显示环境使用。

> 本项目是独立开发的开源项目，与 [Plain Craft Launcher (PCL)](https://github.com/Hex-Dragon/PCL2) 官方无任何隶属或授权关系；开发过程中参考了 PCL 的公开实现思路，但不包含其代码与资源。与 Mojang / Microsoft 同样无任何关联（见文末免责声明）。

## 特性

- **CLI 优先**：整合包导入、实例管理、版本下载、游戏启动全部可在终端完成；TTY 下有交互式选择器与向导（create-vite 风格）
- **整合包导入**：CurseForge / Modrinth / MultiMC / HMCL / PCL 外壳包 / 压缩 `.minecraft` / 纯 Mod 包；自动完成游戏本体、Modloader（Forge / NeoForge / Fabric）与 Mod 下载
- **多实例隔离**：每个实例独立目录；`.incomplete` 标记 + 失败整体回滚，不留半成品
- **原版下载**：任意 MC 版本一键安装，不带参数下载最新正式版；重复执行即校验补齐
- **Java 管理**：自动检测系统 Java、按 MC 版本兼容矩阵选择；缺失时自动从 Adoptium 下载 JRE
- **多玩家档案**：多个离线玩家配置（名称 / 头像 / 皮肤类型），交互式增删改选；支持 authlib-injector 外置登录（如 LittleSkin，登录态加密持久化，启动自动在线刷新）
- **启动稳定性**：自动内存分配（可用内存 50%，上限 16G）、GC 档位、fcitx/ibus XIM 崩溃自动修复（GLFW 3.4 替换）、每次启动落盘完整日志
- **本地开服**：原版/Forge/Fabric/NeoForge 服务端一键安装与前台启动（`server-install`/`server-start`），控制台直通，headless 环境友好
- **QML GUI**（测试版）：与 CLI 共享同一套 SDK（liblpclcore）

## 安装

### 一键安装（推荐）

官方预编译包（内嵌 CurseForge key，完整体验），支持 x86_64（即 amd64）与 macOS（Apple Silicon）。Linux aarch64 用户请走下方源码编译（Qt 官方没有 6.11 的 Linux ARM64 安装包，暂无预编译）：

```bash
curl -fsSL https://github.com/recallrw80-afk/LPCL/releases/latest/download/install.sh | bash
```

国内网络：下载源自动降级——GitHub 优先，不可用自动切 Gitee 镜像，无需参数（测试版加 `--beta`）。

> Gitee 镜像只提供预编译二进制（不提供源码包，也不支持从它源码构建）。源码构建请走 GitHub：`git clone https://github.com/recallrw80-afk/LPCL.git`

装最新**预发布版**（Beta）：注意 `latest` 地址不含预发布，install.sh 需从 tag 地址下载：

```bash
curl -fsSL https://github.com/recallrw80-afk/LPCL/releases/download/v0.1.3-beta/install.sh | bash -s -- --beta
```

（把 `v0.1.3-beta` 换成最新预发布 tag；`--beta` 让脚本走列表接口找到预发布包。）

也可到 [Releases](https://github.com/recallrw80-afk/LPCL/releases/latest) 手动下载压缩包，与 `install.sh` 放一起后 `bash install.sh lpcl-linux-<arch>.tar.xz`。

### 源码编译安装

适合改代码/自定义构建。注意：本地编译的包不内嵌 CF key（CurseForge 自动走 MCIM 镜像），完整体验请用官方包。

```bash
git clone https://github.com/recallrw80-afk/LPCL.git
cd LPCL
make install        # 编译 Release → 零依赖打包 → 安装到本机
```

依赖：Qt 6.11+（Core/Network）、CMake 3.16+、Ninja、C++20 编译器、nlohmann-json 3.11+、ZLIB。Qt 路径默认 `$HOME/Qt/6.11.1/gcc_64`，可用 `make install QT_PREFIX=/path/to/Qt/6.x/gcc_64` 覆盖。

其他电脑编译的包拷到本机：`bash cli/install.sh lpcl-linux-<arch>.tar.xz`。

### 更新与卸载

```bash
lpcl update         # 检查 GitHub Releases 新版本并原地更新（仅 install.sh 安装的副本）
lpcl uninstall      # 卸载（清空游戏目录）
lpcl uninstall -r   # 卸载但保留游戏目录内容
```

### 常用构建命令

```bash
make cli            # 构建 lpcl + liblpclcore.so（Debug）
make package-tar    # 生成零依赖发布包 cli/dist/lpcl-linux-<arch>.tar.xz
make run            # 构建并启动 QML GUI（测试版，二进制名 lpcl-gui）
```

## 快速上手

```bash
# 导入整合包（游戏目录默认程序旁的 ./mc/，可用 set-folder 修改）
lpcl inpack ~/Downloads/某某整合包.zip

# 列出实例并交互式选择启动
lpcl launch

# 或者玩原版
lpcl mc-install        # 最新正式版
lpcl launch 1.20.1
```

完整命令参考见 [cli/README.md](cli/README.md)。任何命令加 `-h` 可打印该命令的参数详解（如 `lpcl inpack -h`）；`lpcl help` 查看命令总表。

## 文档

- [cli/README.md](cli/README.md) — CLI 命令参考、目录结构、FAQ
- [CONTRIBUTING.md](CONTRIBUTING.md) — 开发约定与贡献指南

## 贡献

欢迎 Issue 与 PR。提交前请阅读 [CONTRIBUTING.md](CONTRIBUTING.md)。

## 免责声明

- 本项目不是 Minecraft 官方产品，与 Mojang Studios / Microsoft 没有任何关联，亦未获得其认可或授权。"Minecraft" 是 Mojang Synergies AB 的商标。
- 本项目与 Plain Craft Launcher (PCL) 官方无隶属或授权关系；"LPCL" 仅表示面向 Linux 平台的同类启动器定位。
- 本软件按"原样"提供，作者不对使用本软件造成的任何损失承担责任（详见许可证第 15、16 条）。

## 致谢

- [Plain Craft Launcher (PCL)](https://github.com/Hex-Dragon/PCL2) — 实现思路参考
- [PCL Community Edition](https://github.com/PCL-Community/PCL2-CE) — 部分方案参考（如 MCIM 镜像）
- Qt、nlohmann-json、zlib、Adoptium、BMCLAPI/MCIM 镜像等上游项目

## 许可证

[GNU General Public License v3.0](LICENSE)

版权所有 (C) 2026 LPCL 作者。本程序为自由软件：你可以在自由软件基金会发布的 GNU 通用公共许可证第 3 版条款下再分发和/或修改它。衍生作品必须以相同许可证开源。
