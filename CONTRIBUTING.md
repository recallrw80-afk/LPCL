# 贡献指南

感谢你的兴趣！提交代码前请花两分钟读完本文。

## 基本规则

- 提交信息、代码注释使用中文（变量/函数等标识符保持英文）
- SDK（liblpclcore）新增功能必须同时支持直接调用（`lpcl::` 命名空间，经 `lpcl.h` 暴露）和 CLI 命令；CLI 层只做参数解析与派发，不写业务逻辑
- 整合包导入不是解压复制：必须包含游戏本体 / Modloader / Mod 下载；任一步骤失败 = 整体回滚，不留半成品实例
- 代码改动涉及命令、目录结构、使用方式时，必须同步更新 README 文档
- 排查游戏崩溃（含 native 层 SIGSEGV）禁止归因或建议改动显卡驱动——这类问题一律在启动器侧解决（环境变量、JVM 参数、mod 配置）

## 构建与验证

```bash
cd LPCL
cmake --build cmake-build-debug --target lpcl   # 构建 CLI + SDK
cd cmake-build-debug/cli && ./lpcl test          # 全系统自检（必须全绿）
```

- 改了 QML：对每个 QML 文件跑 qmllint 并消除全部警告（无法消除的加注释说明原因），不允许带警告提交
- 自检 `lpcl test` 必须全部通过

## 提交规范

- 提交信息用中文，简要说明"做了什么 + 为什么"
- 一个提交只做一件事；重构与功能修改分开
- 不要顺手格式化无关代码——保持 diff 可审

## 方向性改动先讨论

以下情况请先开 Issue 讨论再动手：

- 新增命令或改变现有命令语义
- 改动导入管线、启动管线等核心路径
- 引入新的第三方依赖（默认不引入：SDK 只依赖 QtCore/QtNetwork/nlohmann-json/zlib）

## 报告 Bug

请附上：

- `mc/logs/lpcl-launch-*.log`（启动类问题）或命令的完整输出
- 游戏自己的日志 `<实例>/logs/latest.log`（游戏内问题）
- 系统信息：发行版、桌面环境、显卡型号（驱动本身不接受修改建议，见红线）

## 发版（发布维护者）

一条命令：

```bash
./release.sh v0.1.0            # 正式版
./release.sh v0.1.0-Beta       # 预发布（自动标 pre-release，releases/latest 跳过，update 用户不被打扰）
./release.sh v0.1.0 --test     # 先发全量自检再发版
```

**为什么 x86_64 包是本地构建的**：CI 没有 CF key Secret，产物不含内嵌 key（走 MCIM 镜像）；
本地 `.env` 有 key，`release.sh` 用 `LPCL_EMBED_CF_KEY=ON` 构建的 x86_64 包才是「完整体验」。
脚本会校验 key 确实嵌进了二进制（strings 抽查），没嵌进去会中止发版。

流程：检查（gh 已登录/工作区干净/tag 格式 vX.Y.Z）→ 可选自检 → **先打本地 tag**（版本串在编译期由
git describe 注入，必须先有 tag 构建出的二进制才自报 vX.Y.Z；构建失败自动回滚本地 tag）→
本地嵌 key 构建 → 推送 → `gh release create` + 上传本地包和 install.sh。

**aarch64 没有预编译包**：Qt 官方在线仓库没有 6.11.x 的 Linux ARM64 桌面安装包（发行版源最高 6.4，
不满足工程 6.11 要求），CI 无法构建。ARM 用户请走源码编译安装（`git clone` 后 `make install`）。

前置：安装并登录 [gh CLI](https://cli.github.com/)（`gh auth login`）。
重发已推送的 tag 需先删远端 tag 和对应 Release。

## CF API key 轮换（发布维护者）

发布版内嵌的 CurseForge API key 来自 GitHub Secret `LPCL_CURSEFORGE_API_KEY`（CI 在编译期嵌入，fork PR 读不到）。key 泄露或被吊销时按以下流程轮换：

1. 到 <https://console.curseforge.com/> 后台重新生成 key（旧 key 作废）
2. 更新仓库的 GitHub Secret `LPCL_CURSEFORGE_API_KEY`
3. 打新 tag 发版，CI 会用新 key 构建

注意：已发布二进制里的旧 key 无法召回，只能靠第 1 步作废它。key 失效期间用户侧无感——客户端遇 401/403/429 会自动回退 MCIM 镜像。
