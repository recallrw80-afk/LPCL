#include "core/lpclcore_export.h"
#ifndef LPCL_MODPLATFORM_H
#define LPCL_MODPLATFORM_H

#include <QObject>
#include <QString>
#include <QList>
#include <QUrl>
#include <functional>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// Common types

struct ModResource {
    QString id;               // Platform-specific ID
    QString name;             // Display name
    QString summary;          // Short description
    QString description;      // Full description (HTML)
    QString author;           // Author name
    QString iconUrl;          // Icon/logo URL
    QString websiteUrl;       // Project website
    int downloadCount = 0;    // Total downloads
    int category = 0;         // Category ID
    qint64 lastUpdated = 0;   // Unix timestamp
    QStringList versions;     // Supported Minecraft versions
};

struct ModFileInfo {
    QString id;               // File ID
    QString displayName;      // Display name
    QString fileName;         // File name
    QString downloadUrl;      // Direct download URL
    QStringList gameVersions; // Minecraft versions this file supports
    QStringList loaders;      // Mod loaders needed
    qint64 fileSize = 0;      // File size in bytes
    qint64 releaseDate = 0;   // Unix timestamp
    QString sha1;             // SHA1 hash
    bool isRelease = true;    // Release vs beta/alpha
};

/**
 * Unified mod platform API for CurseForge and Modrinth.
 * Mirrors ModDownload.vb from Windows PCL.
 */
class LPCLCORE_EXPORT ModPlatform : public QObject
{
    Q_OBJECT

public:
    enum Platform {
        CurseForge = 0,
        Modrinth = 1
    };

    static ModPlatform& instance();

    // ---- Search ----

    /// Search mods on the given platform
    void searchMods(Platform platform, const QString &query, int page, int pageSize,
                    std::function<void(bool, QList<ModResource>)> onComplete);

    /// Search by category/filters
    void searchByCategory(Platform platform, int category, int page,
                          std::function<void(bool, QList<ModResource>)> onComplete);

    // ---- Mod details ----

    /// Get full mod details
    void getModDetails(Platform platform, const QString &modId,
                       std::function<void(bool, ModResource)> onComplete);

    /// Get available files for a mod
    void getModFiles(Platform platform, const QString &modId,
                     std::function<void(bool, QList<ModFileInfo>)> onComplete);

    // ---- Download ----

    /// Get download URL for a specific file
    QString getFileDownloadUrl(Platform platform, const QString &modId, const QString &fileId);

    /// Download a mod file
    void downloadMod(Platform platform, const QString &modId, const QString &fileId,
                     const QString &savePath,
                     std::function<void(bool, QString)> onComplete,
                     std::function<void(qint64, qint64)> onProgress = nullptr);

    // ---- API Keys ----

    void setCurseForgeApiKey(const QString &key) { m_cfApiKey = key; }
    QString curseForgeApiKey() const { return m_cfApiKey; }

signals:
    void searchProgress(const QString &status);

private:
    ModPlatform() = default;

    // CurseForge API
    void searchCurseForge(const QString &query, int page, int pageSize,
                          std::function<void(bool, QList<ModResource>)> onComplete);
    void getCurseForgeModDetails(const QString &modId,
                                  std::function<void(bool, ModResource)> onComplete);
    void getCurseForgeFiles(const QString &modId,
                             std::function<void(bool, QList<ModFileInfo>)> onComplete);

    // Modrinth API
    void searchModrinth(const QString &query, int page, int pageSize,
                        std::function<void(bool, QList<ModResource>)> onComplete);
    void getModrinthModDetails(const QString &modId,
                                std::function<void(bool, ModResource)> onComplete);
    void getModrinthFiles(const QString &modId,
                           std::function<void(bool, QList<ModFileInfo>)> onComplete);

    // CurseForge 请求辅助：无 API key（或强制镜像）时把官方地址改写为 MCIM 镜像（无需鉴权）
    QString cfApiUrl(const QString &officialUrl) const;
    // 发起 CF API GET 请求：有 key 走官方并附带 x-api-key，无 key / 强制镜像走 MCIM 镜像；
    // 官方返回 401/403/429（key 失效/超限）时自动回退镜像重试一次
    void cfJsonGet(const QString &officialUrl,
                   std::function<void(bool, QString, json)> onComplete);

    QString m_cfApiKey;
    // LPCL_FORCE_CF_MIRROR=1 时跳过官方 API 直连镜像（嵌入 key 失效时的用户逃逸通道）
    bool m_forceCfMirror = false;

    // API base URLs
    static const QString CF_API;
    static const QString CF_MIRROR; // MCIM 镜像，无需 API key
    static const QString MR_API;
};

#endif // LPCL_MODPLATFORM_H
