# lpcl-cli — PCL 命令行启动器

纯 C++ 实现，仅依赖 QtCore + QtNetwork（无 GUI/QML），RSS ~4MB。

## 用法

```
lpcl-cli <命令> [参数]
```

## 全局选项

| 选项 | 说明 |
|---|---|
| `-h`, `--help` | 显示帮助信息 |
| `-v`, `--version` | 显示版本号 |

## 命令

### `list` — 列出已安装版本

```
lpcl-cli list
```

扫描 `.minecraft/versions/` 目录，列出所有已安装的 Minecraft 版本。

**示例：**
```
$ lpcl-cli list
已安装的版本:
  1.21
  1.20.4
  愚者0.22
```

---

### `install <版本号>` — 安装版本

```
lpcl-cli install <版本号>
```

下载并安装指定 Minecraft 版本（含依赖库和资源文件）。

> **状态：** 后端仍为桩实现，暂不可用。

**示例：**
```
$ lpcl-cli install 1.21
正在安装 1.21 ...
```

---

### `launch <版本号>` — 启动游戏

```
lpcl-cli launch <版本号>
```

以离线模式启动指定版本的 Minecraft。自动选择匹配的 Java 运行时。

**前提条件：**
- 目标版本已安装（`lpcl-cli install <版本号>` 或手动放入 `.minecraft/versions/`）
- 系统已安装 Java 运行时（`lpcl-cli list-javas` 查看可用 Java）

> **状态：** 后端仍为桩实现，暂不可用。

**示例：**
```
$ lpcl-cli launch 1.21
正在启动 1.21 ...
[MC] Starting...
游戏已启动，等待退出...
游戏退出，exit code: 0
```

---

### `list-javas` — 列出可用 Java

```
lpcl-cli list-javas
```

扫描系统中所有可用的 Java 运行时。

**示例：**
```
$ lpcl-cli list-javas
检测到的 Java:
  /usr/lib/jvm/java-17-openjdk-amd64
  /usr/lib/jvm/java-21-openjdk-amd64
```

## 环境变量

| 变量 | 说明 | 默认值 |
|---|---|---|
| `HOME` | 用于定位 `.minecraft/` 目录 | — |

## 配置文件

设置存储在 `~/.local/share/lpcl-cli/PCL.ini`（QSettings INI 格式）。

| 键 | 说明 | 默认值 |
|---|---|---|
| `LaunchFolderSelect` | Minecraft 文件夹路径 | `~/.minecraft/` |

## 退出码

| 码 | 说明 |
|---|---|
| 0 | 正常退出 |
| 1 | 参数错误、命令不存在、启动失败 |

## 打包分发

```bash
make package-cli      # → dist/cli/lpcl-cli + liblpclcore.so

# 分发给用户只需这两个文件（放在同一目录）：
dist/cli/
├── lpcl-cli           # 可执行文件（rpath=$ORIGIN）
└── liblpclcore.so     # 核心共享库
```
