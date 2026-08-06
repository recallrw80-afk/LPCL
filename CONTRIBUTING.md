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

## CF API key 轮换（发布维护者）

发布版内嵌的 CurseForge API key 来自 GitHub Secret `LPCL_CURSEFORGE_API_KEY`（CI 在编译期嵌入，fork PR 读不到）。key 泄露或被吊销时按以下流程轮换：

1. 到 <https://console.curseforge.com/> 后台重新生成 key（旧 key 作废）
2. 更新仓库的 GitHub Secret `LPCL_CURSEFORGE_API_KEY`
3. 打新 tag 发版，CI 会用新 key 构建

注意：已发布二进制里的旧 key 无法召回，只能靠第 1 步作废它。key 失效期间用户侧无感——客户端遇 401/403/429 会自动回退 MCIM 镜像。

## Azure 应用注册（微软登录）

`lpcl login` / GUI 微软登录用 OAuth 设备码流程，必须用**自己注册的 Azure 应用**（官方启动器的 client ID 不支持设备码流程，会报 AADSTS700016）。Client ID 是公开信息（出现在用户授权页地址栏），不是秘密，但仍需每个项目各注册各的：

1. 到 <https://portal.azure.com/> → Microsoft Entra ID → 应用注册 → 新注册
2. 受支持账户类型选「任何组织目录中的账户和个人 Microsoft 账户」
3. 注册后进入「身份验证」→ 开启「允许公共客户端流」（设备码流程必需）
4. 复制「应用程序(客户端) ID」

配置方式（任选）：

- 本地开发：写入 `LPCL/.env` 的 `LPCL_MS_CLIENT_ID=`（见 `.env.example` 模板），重新编译即嵌入
- CI 发布：仓库 Secret 配 `LPCL_MS_CLIENT_ID`，workflow 会写入临时 `.env` 嵌入产物

未配置时构建照常成功，仅 `lpcl login` 报「未配置 Client ID」并提示本节的注册步骤。
