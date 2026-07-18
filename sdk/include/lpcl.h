#ifndef LPCLCORE_LPCL_H
#define LPCLCORE_LPCL_H

#include <QString>
#include <QList>
#include <functional>
#include "core/lpclcore_export.h"

namespace lpcl {

// ---- 回调类型（仅异步函数使用） ----

/// 日志回调：(line)
using LogCallback = std::function<void(const QString &line)>;
/// 退出回调：(exitCode)
using ExitCallback = std::function<void(int exitCode)>;

// ---- 结构化数据 ----

/// 当前配置快照
struct ConfigInfo {
    QString version;       // LPCL 版本 (GIT_DESCRIBE)
    QString commit;        // Git commit hash
    QString gameFolder;    // 默认游戏目录
    bool    gameFolderSet; // 是否已设置
    QList<struct PlayerEntry> players;
    QString selectedPlayer; // 当前选中的玩家 UUID
};

/// 导入进度步骤
struct ImportProgress {
    QString step;    // 当前步骤描述
    int     percent; // 0~100
};

/// 玩家配置条目
struct PlayerEntry {
    QString uuid;
    QString name;
    QString avatar;   // 头像路径
    QString skinType; // "slim" / "wide"
};

// ---- SDK API ----

/// 列出已安装的 Minecraft 版本
LPCLCORE_EXPORT QStringList listVersions();

/// 安装指定版本（当前为桩实现）
LPCLCORE_EXPORT bool installVersion(const QString &versionId);

/// 启动游戏（离线模式，异步）
LPCLCORE_EXPORT bool launchVersion(const QString &versionId,
                                    LogCallback onLog = nullptr,
                                    ExitCallback onExit = nullptr);

/// 列出系统中可用的 Java 运行时
LPCLCORE_EXPORT QStringList listJavas();

/// 导入整合包（异步）
LPCLCORE_EXPORT void importModpack(const QString &filePath,
                                    const QString &instanceName,
                                    std::function<void(const ImportProgress &)> onProgress,
                                    std::function<void(bool, const QString &)> onComplete);

/// 删除实例（移除 {mcFolder}/{name}/ 整个目录）
LPCLCORE_EXPORT bool removeInstance(const QString &name);

/// 读取当前配置快照（同步）
LPCLCORE_EXPORT ConfigInfo getConfig();

/// 列出所有玩家配置
LPCLCORE_EXPORT QList<PlayerEntry> listPlayers();

/// 添加玩家配置（返回生成的条目）
LPCLCORE_EXPORT PlayerEntry addPlayer(const QString &name, const QString &avatar = QString());

/// 删除玩家配置
LPCLCORE_EXPORT bool removePlayer(const QString &uuid);

/// 选择当前玩家
LPCLCORE_EXPORT void selectPlayer(const QString &uuid);

} // namespace lpcl

#endif // LPCLCORE_LPCL_H
