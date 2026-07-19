#ifndef LPCL_MODPACK_COMMON_H
#define LPCL_MODPACK_COMMON_H

// 整合包安装内部共享：各安装器与下载管线共用的工具和流程
// 不属于公开 API（公开接口见 sdk/include/modpack.h）

#include <QString>
#include <QStringList>
#include <QList>
#include <nlohmann/json.hpp>
#include "modpack.h"

using json = nlohmann::json;

// Mod 下载条目
struct ModDownloadEntry {
    QString url;       // 直接下载 URL（Modrinth）
    QString savePath;  // 保存到实例目录 + savePath
    QString cfModId;   // CurseForge project ID
    QString cfFileId;  // CurseForge file ID
};

// ---- 通用辅助（common.cpp） ----

QString generateInstanceDir();  // 随机 8 位实例目录名
void writeInstanceMapping(const QString &dirName, const QString &displayName);
json parseJsonSafe(const QByteArray &data, bool *ok = nullptr);
bool copyDir(const QString &src, const QString &dst);
bool extractZip(const QString &zipPath, const QString &destDir,
                PackProgressCallback onProgress, int baseProgress = 10);
QString findMcRoot(const QStringList &entries);
bool validateInstanceName(const QString &name);

// ---- .incomplete 标记与回滚（common.cpp） ----

void markIncomplete(const QString &finalDir);
void markComplete(const QString &finalDir);
void cleanupOnError(const QString &finalDir);  // 删实例目录 + INI 映射
bool checkNameConflict(const QString &targetDir, const QString &name,
                       bool explicitName, PackCompleteCallback onComplete);

// 各安装器共用的前置流程：解析实例名 → 冲突检查 → 建目录 + .incomplete 标记
// 失败时已调 onComplete(false)，返回 false
bool beginInstall(const QString &instanceName, const QString &packName,
                  PackCompleteCallback onComplete,
                  QString &finalDirOut, QString &nameOut);

// 复制失败 = 导入失败（cleanupOnError + onComplete(false)，返回 false）
bool copyOrFail(const QString &src, const QString &finalDir, PackCompleteCallback onComplete);

// ---- 下载管线（pipeline.cpp） ----

void downloadAndFinalize(const QString &mcVersion,
                         const QString &forgeVer, const QString &neoVer, const QString &fabricVer,
                         const QString &finalDir, const QString &name,
                         const QList<ModDownloadEntry> &mods,
                         PackProgressCallback onProgress,
                         PackCompleteCallback onComplete);

// ---- 类型安装器（installers.cpp） ----

void installCurseForge(const QString &filePath, const QString &packDir, const QString &instanceName, PackProgressCallback, PackCompleteCallback);
void installHMCL(const QString &filePath, const QString &packDir, const QString &instanceName, PackProgressCallback, PackCompleteCallback);
void installMultiMC(const QString &filePath, const QString &packDir, const QString &instanceName, PackProgressCallback, PackCompleteCallback);
void installMCBBS(const QString &filePath, const QString &packDir, const QString &instanceName, PackProgressCallback, PackCompleteCallback);
void installModrinth(const QString &filePath, const QString &packDir, const QString &instanceName, PackProgressCallback, PackCompleteCallback);
void installLauncherPack(const QString &filePath, const QString &packDir, const QString &instanceName, PackProgressCallback, PackCompleteCallback);
void installCompressed(const QString &filePath, const QString &packDir, const QString &instanceName, PackProgressCallback, PackCompleteCallback);

#endif // LPCL_MODPACK_COMMON_H
