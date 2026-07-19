#include "lpcl.h"
#include "modpack.h"
#include "core/settings.h"
#include "core/versionmanager.h"
#include "core/javamanager.h"
#include "core/launcher.h"
#include "auth/offlineauth.h"
#include <QCoreApplication>
#include <QDir>
#include <QUuid>

namespace lpcl {

QStringList listVersions() {
    auto &vm = VersionManager::instance();
    vm.loadLocalVersions();
    return vm.versionIds();
}

QStringList listMcVersions() {
    auto &vm = VersionManager::instance();
    vm.loadMcVersions();
    return vm.versionIds();
}

bool installVersion(const QString &versionId) {
    Q_UNUSED(versionId);
    // TODO: Phase 4
    return true;
}

bool launchVersion(const QString &versionId,
                   LogCallback onLog,
                   ExitCallback onExit) {
    auto &launcher = Launcher::instance();
    auto &jm = JavaManager::instance();

    // 解析显示名 → 实例目录名（用于 loadVersion 的路径查找）
    QString dirName = Settings::instance().dirForDisplayName(versionId);
    QString resolvedId = dirName.isEmpty() ? versionId : dirName;

    auto login = OfflineAuth::createOfflineLogin("Player");
    auto version = VersionManager::instance().loadVersion(resolvedId);
    if (jm.javaList().isEmpty()) {
        jm.scanSystemJava();
        jm.waitForScanFinished();
    }
    // 优先按版本兼容矩阵选 Java；无严格匹配时回退到最优可用
    // （如 1.12.2 机器上只有 Java 21/25，严格匹配 Java 8 会落空，但新版本也能跑）
    JavaEntry java = jm.selectJavaForVersion(version);
    if (java.pathJava.isEmpty()) java = jm.selectJava();
    if (java.pathJava.isEmpty()) return false;

    // 注：如在同一进程中多次调用 launchVersion，信号会累积连接。
    // CLI 每次只启动一次游戏即退出，不受影响。
    if (onLog) {
        QObject::connect(&launcher, &Launcher::gameLog,
                         [onLog](const QString &line) { onLog(line); });
    }
    if (onExit) {
        QObject::connect(&launcher, &Launcher::gameExited,
                         [onExit](int code, const QString &) { onExit(code); });
    }

    return launcher.launch(version, java, login);
}

QStringList listJavas() {
    auto &jm = JavaManager::instance();
    jm.scanSystemJava();
    jm.waitForScanFinished();  // 扫描是异步的，读结果前必须等完成
    return jm.javaNames();
}

void importModpack(const QString &filePath,
                    const QString &instanceName,
                    std::function<void(const ImportProgress &)> onProgress,
                    std::function<void(bool, const QString &)> onComplete) {
    // 适配：将 ImportProgress 结构体拆成 (status, percent) 传给内部实现
    auto adaptedProgress = [onProgress](const QString &status, int percent) {
        if (onProgress) onProgress({status, percent});
    };
    installModpack(filePath, instanceName, adaptedProgress, onComplete);
}

bool removeInstance(const QString &name) {
    if (name.isEmpty() || name.contains('/') || name.contains('\\') || name.contains(".."))
        return false;
    QString folder = Settings::instance().getString("LaunchFolderSelect");
    if (!folder.endsWith('/')) folder += '/';

    // 优先从 INI 映射查找随机目录名
    QString dirName = Settings::instance().dirForDisplayName(name);
    if (!dirName.isEmpty()) {
        QString instanceDir = folder + "instances/" + dirName;
        // 先删 INI 映射，再删目录（即使目录删除失败，映射也已清理）
        Settings::instance().removeInstanceDir(dirName);
        return QDir(instanceDir).removeRecursively();
    }

    // 回退：直接用显示名作为目录名（兼容旧格式或测试）
    QString instanceDir = folder + "instances/" + name;
    if (!QDir(instanceDir).exists()) return false;
    bool ok = QDir(instanceDir).removeRecursively();
    // 也尝试清理可能残留的 INI 映射
    Settings::instance().removeInstanceDir(name);
    return ok;
}

ConfigInfo getConfig() {
    ConfigInfo info;
    info.version       = GIT_DESCRIBE;
    info.commit        = GIT_COMMIT_HASH;
    info.gameFolder    = Settings::instance().getString("LaunchFolderSelect");
    info.gameFolderSet = !info.gameFolder.isEmpty();
    info.players       = listPlayers();
    info.selectedPlayer = Settings::instance().selectedPlayer();
    return info;
}

QList<PlayerEntry> listPlayers() {
    QList<PlayerEntry> result;
    QStringList uuids = Settings::instance().playerProfiles();
    for (const auto &uuid : uuids) {
        PlayerEntry e;
        e.uuid     = uuid;
        e.name     = Settings::instance().getProfile(uuid, "Name");
        e.avatar   = Settings::instance().getProfile(uuid, "Avatar");
        e.skinType = Settings::instance().getProfile(uuid, "SkinType", "slim");
        result.append(e);
    }
    return result;
}

PlayerEntry addPlayer(const QString &name, const QString &avatar) {
    QString uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    Settings::instance().setProfile(uuid, "Name", name);
    Settings::instance().setProfile(uuid, "Avatar", avatar);
    Settings::instance().setProfile(uuid, "SkinType", "slim");

    // 首个玩家自动选中
    if (Settings::instance().playerProfiles().size() == 1)
        Settings::instance().selectPlayer(uuid);

    return {uuid, name, avatar, "slim"};
}

bool removePlayer(const QString &uuid) {
    if (uuid.isEmpty()) return false;
    if (!Settings::instance().playerProfiles().contains(uuid)) return false;
    Settings::instance().removeProfile(uuid);
    if (Settings::instance().selectedPlayer() == uuid)
        Settings::instance().selectPlayer(QString());
    return true;
}

bool selectPlayer(const QString &uuid) {
    if (!Settings::instance().playerProfiles().contains(uuid)) return false;
    Settings::instance().selectPlayer(uuid);
    return true;
}

} // namespace lpcl
