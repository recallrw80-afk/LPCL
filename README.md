# LPCL — Linux Plain Craft Launcher

一个用 C++20 / Qt 6 编写的跨平台 Minecraft 启动器，提供轻量命令行前端 `lpcl-cli`（RSS ~4MB），适合桌面、服务器与无显示环境使用。

> 本项目是独立开发的开源项目，与 [Plain Craft Launcher (PCL)](https://github.com/Hex-Dragon/PCL2) 官方无任何隶属或授权关系；开发过程中参考了 PCL 的公开实现思路，但不包含其代码与资源。与 Mojang / Microsoft 同样无任何关联（见文末免责声明）。

## 特性

- **CLI 优先**：整合包导入、实例管理、版本下载、游戏启动全部可在终端完成；TTY 下有交互式选择器与向导（create-vite 风格）
- **整合包导入**：CurseForge / Modrinth / MultiMC / HMCL / PCL 外壳包 / 压缩 `.minecraft` / 纯 Mod 包；自动完成游戏本体、Modloader（Forge / NeoForge / Fabric）与 Mod 下载
- **多实例隔离**：每个实例独立目录；`.incomplete` 标记 + 失败整体回滚，不留半成品
- **原版下载**：任意 MC 版本一键安装，不带参数下载最新正式版；重复执行即校验补齐
- **Java 管理**：自动检测系统 Java、按 MC 版本兼容矩阵选择；缺失时自动从 Adoptium 下载 JRE
- **多玩家档案**：多个离线玩家配置（名称 / 头像 / 皮肤类型），交互式增删改选
- **启动稳定性**：自动内存分配（可用内存 50%，上限 16G）、GC 档位、fcitx/ibus XIM 崩溃自动修复（GLFW 3.4 替换）、每次启动落盘完整日志
- **QML GUI**（开发中）：与 CLI 共享同一套 SDK（liblpclcore）

## 安装

### 预编译包（待发布）

发布页将提供各架构压缩包与一键安装脚本：

```bash
curl -fsSL <发布地址>/install.sh | bash
```

### 源码构建

依赖：Qt 6.11+（Core/Network/Quick）、CMake 3.16+、Ninja、C++20 编译器、nlohmann-json 3.11+、ZLIB。

```bash
cd LPCL
make cli            # 构建 lpcl-cli + liblpclcore.so（Debug）
make package-cli    # Release 打包到 dist/cli/，可独立分发
make run            # QML GUI（开发中）
```

## 快速上手

```bash
# 导入整合包（游戏目录默认程序旁的 ./mc/，可用 set-folder 修改）
lpcl-cli inpack ~/Downloads/某某整合包.zip

# 列出实例并交互式选择启动
lpcl-cli launch

# 或者玩原版
lpcl-cli mc-install        # 最新正式版
lpcl-cli launch 1.20.1
```

完整命令参考见 [cli/README.md](cli/README.md)，或直接 `lpcl-cli help`。

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
