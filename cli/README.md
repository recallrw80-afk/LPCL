# lpcl-cli — LPCL 命令行启动器

纯 C++ 命令行 Minecraft 启动器，链接 `liblpclcore`（QtCore + QtNetwork，无 GUI/QML 依赖），RSS 基线 ~13MB。

## 构建与运行

```bash
cd LPCL
make cli                 # 构建 liblpclcore.so + lpcl-cli（Debug）
make run-cli ARGS=list   # 构建并运行
make package-cli         # Release 打包 → dist/cli/（lpcl-cli + liblpclcore.so，rpath=$ORIGIN，可独立分发）
```

产物：`LPCL/cmake-build-debug/cli/lpcl-cli`（默认游戏目录为可执行文件旁的 `./mc/`，可用 `set-folder` 修改）。

## 命令一览

### 实例管理

| 命令 | 说明 |
|---|---|
| `list` | 列出已导入的整合包实例 |
| `list-rm <名称\|*>` | 删除实例（`*` 清空全部，需加引号防 shell 展开） |
| `mc-list` | 列出原版 MC 版本（全局 `versions/`） |
| `launch [名称]` | 启动游戏；省略名称时列出实例按序号选择 |

### 导入与下载

| 命令 | 说明 |
|---|---|
| `inpack <文件> [--r <名称>] [--to <实例>] [--folder <路径>]` | 导入整合包 |
| `install <版本>` | 下载原版 MC 版本（json/jar/libraries/assets/natives；重复调用即校验补齐） |
| `install-java <大版本>` | 从 Adoptium 下载安装 JRE 到 `{mcFolder}/javas/` 并注册 |

`inpack` 支持的类型：CurseForge / HMCL / MultiMC / MCBBS / Modrinth / LauncherPack（外壳包）/ Compressed `.minecraft` / **Mod 包**（仅 jar，需 `--to <实例名>` 指定目标实例）。

### Java

| 命令 | 说明 |
|---|---|
| `list-javas` | 列出可用 Java（PATH、JAVA_HOME、系统目录、`{mcFolder}/javas/`） |

启动时按 MC 版本兼容矩阵自动选择 Java（区间内取最低满足版本）；一个 Java 都没有时自动从 Adoptium 下载所需 JRE。

### 玩家配置

| 命令 | 说明 |
|---|---|
| `player-add <名称> [--avatar <路径>]` | 添加玩家（生成 UUID，首个自动选中） |
| `player-rm <uuid>` | 删除玩家 |
| `player-list` | 列出玩家（`*` 标记当前选中） |
| `player-select <uuid>` | 选择当前玩家 |

启动使用选中的玩家 Profile（离线登录）。

### 配置与信息

| 命令 | 说明 |
|---|---|
| `set-folder <路径>` | 设置默认游戏目录（持久保存） |
| `set-player <名称>` | 设置玩家名称 |
| `set-lang <en\|zh>` | 设置界面语言（持久保存，默认英文） |
| `config` | 查看当前配置（版本、目录、玩家、实例映射） |
| `test` | 全系统自检（纯函数单测 + 合成包端到端冒烟） |
| `help` / `version` | 帮助 / 版本号 |

无任何 `--` 选项 flag；一次性覆盖游戏目录用 `inpack ... --folder <路径>`（不写回配置）。

## 存储模型

```
{mcFolder}/
├── instances/{随机8位}/        ← 整合包实例（mods/config/PCL/版本文件）
│   └── .incomplete             ← 导入中标记（成功即删，失败回滚删除整个实例）
├── versions/                   ← 全局 MC 版本（原版 + loader 安装的版本）
├── libraries/                  ← 全局共享库（含整合包合并来的依赖）
├── assets/                     ← 全局共享资源（indexes/objects）
├── javas/                      ← 自动下载的 JRE
└── tmp/                        ← 导入临时文件（自动清理）
```

实例显示名 ↔ 随机目录名的映射存在 `LPCL.ini`（QSettings group 操作）；`list` 只显示映射存在且目录真实的实例，失效映射自动清理。

## 关键行为约定

- **导入必须含下载**：MC 本体 + natives + modloader（Forge/NeoForge/Fabric）+ mod（CurseForge/Modrinth）
- **失败即回滚**：任一下载步骤失败（MC/natives/modloader/mod）= 整个导入失败，删除半成品实例
- **启动预检**：launch 前校验主 jar/libraries/assets/natives，vanilla 缺失自动补齐，loader 库缺失明确报错
- **CurseForge key**：环境变量 `LPCL_CURSEFORGE_API_KEY` → Settings（加密）→ `LPCL/.env` 编译期嵌入（发布构建不含）→ 均无则走 MCIM 镜像（`mod.mcimirror.top`，无需 key）；forgecdn 下载失败自动回退镜像
- **语言**：`set-lang zh` 后全中文输出

## 目录结构（本目录）

```
cli/
├── main.cpp        # 入口、选项解析、命令 handler、dispatchCommand、printHelp
├── test.cpp        # test 自检子系统（单测 + 合成包冒烟）
├── i18n.h          # 中英文切换共享
├── CMakeLists.txt
└── dist/           # package-cli 产物
```

业务逻辑一律在 `sdk/`（liblpclcore，`lpcl::` 命名空间返回结构化数据），CLI 只做参数解析和派发。
