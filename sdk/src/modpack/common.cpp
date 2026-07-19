// 安装器/管线共享的内部工具实现
#include "modpack_common.h"
#include "core/settings.h"
#include "core/versionmanager.h"
#include "util/file_utils.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QRandomGenerator>

// 生成随机 8 位实例目录名（字母 + 数字）
QString generateInstanceDir() {
    static const QString chars = QStringLiteral("abcdefghijklmnopqrstuvwxyz0123456789");
    QString result;
    result.reserve(8);
    for (int i = 0; i < 8; ++i)
        result += chars[QRandomGenerator::global()->bounded(chars.size())];
    return result;
}

// 写入 INI 实例映射（dirName → displayName）
void writeInstanceMapping(const QString &dirName, const QString &displayName) {
    Settings::instance().setInstanceDir(dirName, displayName);
}

json parseJsonSafe(const QByteArray &data, bool *ok) {
    try {
        if (ok) *ok = true;
        return json::parse(data.toStdString());
    } catch (const json::parse_error &e) {
        if (ok) *ok = false;
        return json::object();  // empty object on failure
    }
}

bool copyDir(const QString &src, const QString &dst) {
    return FileUtils::copyDir(src, dst);
}

// ---- extract zip ----

bool extractZip(const QString &zipPath, const QString &destDir,
                       PackProgressCallback onProgress, int baseProgress) {
    QDir().mkpath(destDir);
    if (onProgress) onProgress("Extracting...", baseProgress);
    return FileUtils::extractZip(zipPath, destDir);
}

// ---- find .minecraft root inside archive ----

QString findMcRoot(const QStringList &entries) {
    // 搜索 */versions/X/X.json 模式
    QRegularExpression re(R"((.+/)versions/[^/]+/[^/]+\.json)");
    for (const auto &e : entries) {
        auto match = re.match(e);
        if (match.hasMatch())
            return match.captured(1);  // the prefix before versions/
    }
    return "";
}

// 校验实例名：拒绝空名、路径分隔符、路径穿越
bool validateInstanceName(const QString &name) {
    if (name.isEmpty()) return false;
    if (name.contains('/') || name.contains('\\')) return false;
    if (name.contains("..")) return false;
    if (name == ".") return false;
    return true;
}

// 获取导入临时工作目录
// 标记实例目录为"导入中"（.incomplete 文件），完成后删除
void markIncomplete(const QString &finalDir) {
    QDir().mkpath(finalDir);
    QFile marker(finalDir + ".incomplete");
    (void)marker.open(QIODevice::WriteOnly | QIODevice::Truncate);
    marker.close();
}
void markComplete(const QString &finalDir) {
    QFile::remove(finalDir + ".incomplete");
}

// 导入失败时回滚：删实例目录 + 清理 INI 映射
void cleanupOnError(const QString &finalDir) {
    QString dirName = QDir(finalDir).dirName();
    QDir(finalDir).removeRecursively();
    if (!dirName.isEmpty())
        Settings::instance().removeInstanceDir(dirName);
}

bool checkNameConflict(const QString &targetDir, const QString &name,
                               bool explicitName, PackCompleteCallback onComplete) {
    if (!validateInstanceName(name)) {
        if (onComplete)
            onComplete(false, QString("Invalid instance name: \"%1\"").arg(name));
        return false;
    }
    // 检查显示名冲突：映射存在且实例目录在当前游戏目录真实存在才算
    // （目录已丢失的失效映射不算冲突——list 会自清理）
    QString existingDir = Settings::instance().dirForDisplayName(name);
    if (!existingDir.isEmpty() &&
        QDir(VersionManager::instance().mcFolder() + "instances/" + existingDir + "/").exists()) {
        if (explicitName) {
            if (onComplete)
                onComplete(false, QString("Instance \"%1\" already exists, use a different name").arg(name));
            return false;
        }
        if (onComplete)
            onComplete(false, QString("Instance \"%1\" already exists, use --r <name>").arg(name));
        return false;
    }
    // 检查随机目录名是否碰巧存在（极端罕见，安全网）
    if (QDir(targetDir).exists()) {
        if (onComplete)
            onComplete(false, "Internal error: directory name collision, please retry");
        return false;
    }
    return true;
}

// 各安装器共用的前置流程：解析实例名 → 冲突检查 → 建目录 + .incomplete 标记
bool beginInstall(const QString &instanceName, const QString &packName,
                  PackCompleteCallback onComplete,
                  QString &finalDirOut, QString &nameOut) {
    QString name = instanceName.isEmpty() ? packName : instanceName;
    QString instanceDir = generateInstanceDir();
    QString finalDir = VersionManager::instance().mcFolder() + "instances/" + instanceDir + "/";
    if (!checkNameConflict(finalDir, name, !instanceName.isEmpty(), onComplete)) return false;
    markIncomplete(finalDir);
    finalDirOut = finalDir;
    nameOut = name;
    return true;
}

// 复制失败 = 导入失败（回滚 + 报错）
bool copyOrFail(const QString &src, const QString &finalDir, PackCompleteCallback onComplete) {
    if (copyDir(src, finalDir)) return true;
    cleanupOnError(finalDir);
    if (onComplete) onComplete(false, "Copy failed: " + src);
    return false;
}

// 提取纯净 MC 版本号（去掉 modloader 前缀）
QString extractVanillaVersion(const QString &v) {
    QRegularExpression re(R"(^\d+\.\d+(?:\.\d+)?)");
    auto m = re.match(v);
    return m.hasMatch() ? m.captured(0) : v;
}

// 确定实例应记录的版本 json 名
QString resolveInstanceVersionName(const QString &finalDir, const QString &mcVersion) {
    // 实例内版本文件夹优先（LauncherPack/Compressed 的包自带版本）
    QDir instVd(finalDir + "versions/");
    for (const auto &entry : instVd.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        if (QFile::exists(entry.absoluteFilePath() + "/" + entry.fileName() + ".json"))
            return entry.fileName();
    }
    if (!mcVersion.isEmpty()) {
        QString vanilla = extractVanillaVersion(mcVersion);
        // 全局 versions/ 下找 vanilla 前缀的 loader 目录（如 1.20.1-forge-47.4.10）
        QDir gd(VersionManager::instance().mcFolder() + "versions/");
        for (const auto &entry : gd.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            if (entry.fileName().startsWith(vanilla + "-") &&
                QFile::exists(entry.absoluteFilePath() + "/" + entry.fileName() + ".json"))
                return entry.fileName();
        }
        return vanilla;
    }
    return {};
}
