// 整合包导入入口：类型检测 → 解压到 tmp/pack/ → 派发到类型安装器
#include "modpack_common.h"
#include "core/versionmanager.h"
#include "download/downloadmanager.h"
#include <QDir>
#include <QFile>

// ---- internal dispatch (from already-extracted directory) ----

static void installModpackFromDir(const QString &filePath, const QString &packDir,
                                   PackType type, const QString &instanceName,
                                   const QString &targetInstance,
                                   PackProgressCallback onProgress,
                                   PackCompleteCallback onComplete) {
    // 处理一级目录包装（如 zip/蛊真人/ → 进入子目录）
    QString effectiveDir = packDir;
    QDir rootDir(effectiveDir);
    auto entries = rootDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    if (entries.size() == 1) {
        QString subDir = entries.first().absoluteFilePath() + "/";
        QDir sub(subDir);
        if (sub.exists("manifest.json") || sub.exists("modrinth.index.json") ||
            sub.exists("mmc-pack.json") || sub.exists("mcbbs.packmeta") ||
            sub.exists("modpack.json") || sub.exists("modpack.zip") ||
            sub.exists("modpack.mrpack")) {
            effectiveDir = subDir;
        }
    }

    // 分派到对应的安装器
    switch (type) {
    case PackType::CurseForge:
        installCurseForge(filePath, effectiveDir, instanceName, onProgress, onComplete);
        break;
    case PackType::HMCL:
        installHMCL(filePath, effectiveDir, instanceName, onProgress, onComplete);
        break;
    case PackType::MultiMC:
        installMultiMC(filePath, effectiveDir, instanceName, onProgress, onComplete);
        break;
    case PackType::MCBBS:
        installMCBBS(filePath, effectiveDir, instanceName, onProgress, onComplete);
        break;
    case PackType::Modrinth:
        installModrinth(filePath, effectiveDir, instanceName, onProgress, onComplete);
        break;
    case PackType::LauncherPack:
        installLauncherPack(filePath, effectiveDir, instanceName, onProgress, onComplete);
        break;
    case PackType::Mod:
        installMod(effectiveDir, targetInstance, onProgress, onComplete);
        break;
    case PackType::Compressed:
        installCompressed(filePath, effectiveDir, instanceName, onProgress, onComplete);
        break;
    default:
        // 回退：当作压缩版 .minecraft 处理
        installCompressed(filePath, effectiveDir, instanceName, onProgress, onComplete);
        break;
    }
}

// ---- main entry ----

void installModpack(const QString &filePath,
                     const QString &instanceName,
                     const QString &targetInstance,
                     PackProgressCallback onProgress,
                     PackCompleteCallback onComplete) {
    if (!QFile::exists(filePath)) {
        if (onComplete) onComplete(false, "File not found: " + filePath);
        return;
    }

    if (onProgress) onProgress("Detecting modpack type...", 5);

    PackType type = detectPackType(filePath);
    if (onProgress) onProgress("Type: " + packTypeName(type), 8);

    // 在游戏目录下创建 tmp 目录，解压到 tmp/pack/
    QString mcFolder = VersionManager::instance().mcFolder();
    // 设置 mcFolder 供 AssetDownloader 兜底使用
    DownloadManager::instance().setProperty("mcFolder", mcFolder);
    QString tmpRoot = mcFolder + "tmp/";

    // 清理旧 tmp，确保干净环境
    QDir(tmpRoot).removeRecursively();
    QDir().mkpath(tmpRoot);

    QString packDir = tmpRoot + "pack/";
    QDir().mkpath(packDir);

    if (!extractZip(filePath, packDir, onProgress, 10)) {
        if (onComplete) onComplete(false, "Extraction failed");
        QDir(tmpRoot).removeRecursively();
        return;
    }

    installModpackFromDir(filePath, packDir, type, instanceName, targetInstance, onProgress, onComplete);

    // 注意：tmp/ 不在此清理——异步下载链末尾的 finalizeNow() 会统一清理；
    // 同步路径（Compressed Step 3）已自行清理。
}
