#ifndef LPCLCORE_LPCL_H
#define LPCLCORE_LPCL_H

#include <QString>
#include <functional>
#include "core/lpclcore_export.h"

namespace lpcl {

/// 日志回调：(line)
using LogCallback = std::function<void(const QString &line)>;
/// 退出回调：(exitCode)
using ExitCallback = std::function<void(int exitCode)>;

/// 列出已安装的 Minecraft 版本
LPCLCORE_EXPORT QStringList listVersions();

/// 安装指定版本（当前为桩实现）
LPCLCORE_EXPORT bool installVersion(const QString &versionId);

/// 启动游戏（离线模式）
LPCLCORE_EXPORT bool launchVersion(const QString &versionId,
                                    LogCallback onLog = nullptr,
                                    ExitCallback onExit = nullptr);

/// 列出系统中可用的 Java 运行时
LPCLCORE_EXPORT QStringList listJavas();

} // namespace lpcl

#endif // LPCLCORE_LPCL_H
