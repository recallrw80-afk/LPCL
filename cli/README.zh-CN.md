# lpcl — Linux 上的 Minecraft 启动器

[English](README.md) | **简体中文**

**LPCL（Linux Plain Craft Launcher）** 是 [Plain Craft Launcher (PCL)](https://github.com/Hex-Dragon/PCL2) 的跨平台移植版——一个用 C++/Qt 编写的 Minecraft 启动器。`lpcl` 是它的命令行前端：不依赖图形界面，几 MB 内存即可运行，支持整合包导入、多版本管理和游戏启动。

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

### 源码编译安装（推荐）

克隆正式版本代码后编译并安装到本机（`~/.local/lib/lpcl/` + `~/.local/bin/lpcl`）：

```bash
git clone https://github.com/recallrw80-afk/LPCL.git
cd LPCL
make install        # 编译 Release → 零依赖打包 → 安装到本机
```

依赖：Qt 6.11+（Core/Network）、CMake 3.16+、Ninja、C++20 编译器、nlohmann-json 3.11+、ZLIB。Qt 路径默认 `$HOME/Qt/6.11.1/gcc_64`，可用 `make install QT_PREFIX=/path/to/Qt/6.x/gcc_64` 覆盖。

完成后任意目录直接：

```bash
lpcl help
```

### 其他电脑编译的包安装到本机

在编译机上 `make package-tar` 生成零依赖压缩包 `cli/dist/lpcl-linux-<arch>.tar.gz`，拷贝到目标机后：

```bash
bash install.sh lpcl-linux-x86_64.tar.gz
```

（`install.sh` 就在仓库 `cli/` 下；没有仓库也可以从发布页获取。）

### 一键安装

```bash
curl -fsSL https://github.com/recallrw80-afk/LPCL/releases/latest/download/install.sh | bash
```

### 更新与卸载

```bash
lpcl update         # 检查 GitHub Releases 新版本并原地更新（仅 install.sh 安装的副本）
lpcl uninstall      # 卸载（清空游戏目录）
lpcl uninstall -r   # 卸载但保留游戏目录内容
```

### 常用构建命令

```bash
make cli            # 构建 lpcl + liblpclcore.so（Debug）
make package-tar    # 生成零依赖发布包 cli/dist/lpcl-linux-<arch>.tar.gz
make run            # 构建并启动 QML GUI（测试版，二进制名 lpcl-gui）
```

## 快速上手

```bash
# 1. 设置游戏目录（默认是程序旁的 ./mc/，可省略）
lpcl set-folder /home/yourname/mc

# 2. 导入一个整合包
lpcl inpack ~/Downloads/某某整合包.zip

# 3. 看看有哪些实例
lpcl list

# 4. 启动！
lpcl launch
# 或者直接指定：
lpcl launch 实例名
```

没有整合包？先装个原版：

```bash
lpcl mc-install          # 最新正式版
lpcl mc-install 1.20.1   # 指定版本
lpcl launch 1.20.1
```

## 命令参考

### 实例

| 命令 | 说明 |
|---|---|
| `list` | 列出所有实例 |
| `mods <名称>` | 列出实例的 Mod（文件大小 + 启用/禁用状态） |
| `list-rm [名称\|*]` | 删除实例（无参时上下键选择 + 二次确认；`*` 删除全部，注意加引号） |
| `mc-list` | 列出已下载的原版 MC 版本 |
| `launch [名称]` | 启动游戏；不填名称时**上下键选择**（列表含实例 + 原版/加载器版本；非 TTY 退回输序号） |

### 下载与导入

| 命令 | 说明 |
|---|---|
| `inpack <文件> [--r <名称>] [--to <实例>] [--folder <路径>]` | 导入整合包；`--r` 重命名实例；Mod 包需 `--to` 指定目标实例 |
| `mc-install [版本]` | 下载原版 MC 版本（不带参数为最新正式版） |
| `java-install <大版本>` | 下载安装 Java（Adoptium JRE） |

### Java

| 命令 | 说明 |
|---|---|
| `list-javas` | 列出检测到的 Java |

### 玩家

| 命令 | 说明 |
|---|---|
| `player-add [名称] [--avatar <路径>] [--skin <slim\|wide\|default>]` | 添加玩家；不带参数进入交互向导（名字 → 皮肤 → 头像 → 高级配置自定义 UUID） |
| `player-edit [uuid\|序号]` | 修改玩家配置（交互向导，回车保留原值；无参时上下键选择玩家） |
| `player-rm [uuid\|序号]` | 删除玩家（无参时上下键选择 + 二次确认） |
| `player-list` | 列出玩家（带序号，`*` 为当前选中） |
| `player-select <uuid\|序号>` | 选择当前玩家（可用序号；无参时上下键选择） |

### 配置与其他

| 命令 | 说明 |
|---|---|
| `set-folder <路径>` | 设置默认游戏目录 |
| `set-lang <en\|zh>` | 设置界面语言（默认英文） |
| `set-mem <MB\|auto>` | 设置游戏最大内存；`auto`（默认）按可用内存 50% 自动分配（上限 16G） |
| `login [服务器] [邮箱]` | 外置登录（authlib-injector，如 LittleSkin；token 加密持久保存，启动自动在线刷新） |
| `logout` | 退出外置登录，回退离线玩家 |
| `update` | 检查 GitHub Releases 是否有新版本并自动更新（仅 install.sh 安装的副本；`LPCL_REPO` 可换源） |
| `uninstall [-r]` | 卸载启动器；`-r` 保留游戏目录内容 |
| `config` | 查看当前配置 |
| `report [描述]` | 生成 GitHub Issue 预填链接（自动附环境信息+最近启动日志，已脱敏） |
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
├── javas/          # 自动下载的 Java
└── logs/           # 启动器日志（lpcl-launch-*.log，滚动保留 10 份）
```

游戏自己的日志在 `<实例>/logs/latest.log`（MC/mod 标准位置）；启动器每次 `launch` 另写一份完整会话日志（启动命令行 + 全部游戏输出 + 退出码）到 `mc/logs/lpcl-launch-<时间戳>.log`，排查启动问题看这个。

## 常见问题

**导入 CurseForge 整合包需要 API key 吗？**
不需要。官方发布的预编译包内嵌了 key（走官方 API，最快最稳）；源码构建默认不带 key，自动使用 MCIM 镜像下载。key 失效或被吊销时会自动回退镜像重试，无需手动处理。

**界面怎么变成中文？**
`lpcl set-lang zh`（一次设置，永久生效）。

**有图形界面吗？**
QML GUI 目前是测试版，与 CLI 共享同一套 SDK。开发构建后用 `make run` 启动（二进制名 `lpcl-gui`）；发布包如包含 GUI 会注册 `lpcl-gui` 命令。

**导入失败会怎样？**
任一下载环节失败（游戏文件/Modloader/Mod）都会整体回滚，不会留下装了一半的实例，重试即可。

## 许可与声明

本项目以 [GNU GPL v3](../LICENSE) 开源。本项目是独立开发的开源软件：与 Mojang / Microsoft 无关联（"Minecraft" 是 Mojang Synergies AB 的商标），与 [PCL](https://github.com/Hex-Dragon/PCL2) 官方亦无隶属或授权关系。按"原样"提供，不承担使用责任（见许可证第 15、16 条）。
