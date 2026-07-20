# lpcl-cli — Linux 上的 Minecraft 启动器

**LPCL（Linux Plain Craft Launcher）** 是 [Plain Craft Launcher (PCL)](https://github.com/Hex-Dragon/PCL2) 的跨平台移植版——一个用 C++/Qt 编写的 Minecraft 启动器。`lpcl-cli` 是它的命令行前端：不依赖图形界面，几 MB 内存即可运行，支持整合包导入、多版本管理和游戏启动。

## 功能特性

- **整合包导入**：CurseForge / Modrinth / MCBBS / MultiMC / HMCL / 启动器外壳包 / 压缩 `.minecraft` / 纯 Mod 包，自动完成游戏本体、Modloader（Forge/NeoForge/Fabric）、Mod 下载
- **多实例管理**：每个实例独立目录，互不干扰；支持批量删除
- **原版下载**：任意 MC 版本一键下载安装、校验补齐
- **Java 管理**：自动检测系统 Java，按 MC 版本自动选择；缺失时自动从 Adoptium 下载安装
- **多玩家配置**：多个离线玩家档案，支持皮肤类型（slim/wide）
- **断点续传与校验**：所有下载按 SHA1 校验，重复操作自动跳过已有文件
- **中文/英文界面**：`set-lang zh` 一键切换中文

## 环境要求

- Linux（x86_64 / aarch64）
- 运行游戏需要 Java 运行时（没有也没关系，启动时会自动下载）

## 安装

### 一键安装（推荐）

```bash
curl -fsSL <发布地址>/install.sh | bash
```

脚本自动完成：下载对应架构的包 → 解压到 `~/.local/lib/lpcl/` → 在 `~/.local/bin/` 注册 `lpcl-cli` 命令。完成后任意目录直接：

```bash
lpcl-cli help
```

### 源码构建

```bash
cd LPCL
make cli            # 构建 lpcl-cli + liblpclcore.so
make package-cli    # 打包到 dist/cli/，可拷贝到任意位置独立运行
make package-tar    # 生成发布压缩包 dist/lpcl-cli-linux-<arch>.tar.gz
```

## 快速上手

```bash
# 1. 设置游戏目录（默认是程序旁的 ./mc/，可省略）
./lpcl-cli set-folder /home/yourname/mc

# 2. 导入一个整合包
./lpcl-cli inpack ~/Downloads/某某整合包.zip

# 3. 看看有哪些实例
./lpcl-cli list

# 4. 启动！
./lpcl-cli launch
# 或者直接指定：
./lpcl-cli launch 实例名
```

没有整合包？先装个原版：

```bash
./lpcl-cli mc-install 1.20.1
./lpcl-cli launch 1.20.1
```

## 命令参考

### 实例

| 命令 | 说明 |
|---|---|
| `list` | 列出所有实例 |
| `list-rm <名称\|*>` | 删除实例（`*` 删除全部，注意加引号） |
| `mc-list` | 列出已下载的原版 MC 版本 |
| `launch [名称]` | 启动游戏；不填名称则从列表中选择 |

### 下载与导入

| 命令 | 说明 |
|---|---|
| `inpack <文件> [--r <名称>] [--to <实例>] [--folder <路径>]` | 导入整合包；`--r` 重命名实例；Mod 包需 `--to` 指定目标实例 |
| `mc-install <版本>` | 下载原版 MC 版本 |
| `java-install <大版本>` | 下载安装 Java（Adoptium JRE） |

### Java

| 命令 | 说明 |
|---|---|
| `list-javas` | 列出检测到的 Java |

### 玩家

| 命令 | 说明 |
|---|---|
| `player-add <名称> [--avatar <路径>] [--skin <slim\|wide\|default>]` | 添加玩家 |
| `player-rm <uuid\|序号>` | 删除玩家（可用 `player-list` 里的序号） |
| `player-list` | 列出玩家（带序号，`*` 为当前选中） |
| `player-select <uuid\|序号>` | 选择当前玩家（可用序号） |

### 配置与其他

| 命令 | 说明 |
|---|---|
| `set-folder <路径>` | 设置默认游戏目录 |
| `set-player <名称>` | 设置玩家名称 |
| `set-lang <en\|zh>` | 设置界面语言（默认英文） |
| `update` | 检查 GitHub Releases 是否有新版本并自动更新 |
| `uninstall [-r]` | 卸载启动器；`-r` 保留游戏目录内容 |
| `config` | 查看当前配置 |
| `test` | 全系统自检 |
| `help` / `version` | 帮助 / 版本号 |

## 游戏文件放在哪

默认在启动器旁的 `mc/` 目录（可用 `set-folder` 修改）：

```
mc/
├── instances/      # 你导入的每个整合包实例（独立目录）
├── versions/       # 下载的 MC 版本
├── libraries/      # 游戏依赖库（多实例共享）
├── assets/         # 游戏资源（多实例共享）
└── javas/          # 自动下载的 Java
```

## 常见问题

**导入 CurseForge 整合包需要 API key 吗？**
不需要。未配置 key 时自动使用 MCIM 镜像下载。有 key 的话可以通过环境变量 `LPCL_CURSEFORGE_API_KEY` 配置走官方 API。

**界面怎么变成中文？**
`lpcl-cli set-lang zh`（一次设置，永久生效）。

**导入失败会怎样？**
任一下载环节失败（游戏文件/Modloader/Mod）都会整体回滚，不会留下装了一半的实例，重试即可。

## 许可

本项目基于 Qt（LGPL）构建。Windows 原版 [PCL2](https://github.com/Hex-Dragon/PCL2) 与本项目的关系见其原仓库说明。
