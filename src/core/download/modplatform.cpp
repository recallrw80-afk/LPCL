#include "download/modplatform.h"
#include "download/downloadmanager.h"
#include "core/settings.h"
#include "util/crypto_utils.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrlQuery>
#include <QLoggingCategory>

static Q_LOGGING_CATEGORY(logMod, "lpcl.mod")

const QString ModPlatform::CF_API = "https://api.curseforge.com/v1";
const QString ModPlatform::MR_API = "https://api.modrinth.com/v2";

ModPlatform& ModPlatform::instance() {
    static ModPlatform m;
    // Initialize CurseForge API key from settings or default
    // PCL uses a compiled-in key; LPCL reads from settings first
    if (m.m_cfApiKey.isEmpty()) {
        // Try settings first
        QString key = Settings::instance().getString("CurseForgeApiKey", "");
        if (!key.isEmpty()) {
            m.m_cfApiKey = CryptoUtils::pclDecrypt(key);
        }
        // Fallback: default key (same as PCL2 open-source distribution)
        if (m.m_cfApiKey.isEmpty()) {
            m.m_cfApiKey = "$2a$10$wAadBaDMFGiEYjFJQOYiKOKz1OzIj1mVj2jYjJfJjJfJjJfJjJfJj";
            // NOTE: Replace with a valid CurseForge Core API key from https://console.curseforge.com
            // The above placeholder will cause CF requests to fail with 401
        }
    }
    return m;
}

// Public API — search

void ModPlatform::searchMods(Platform platform, const QString &query, int page, int pageSize,
                               std::function<void(bool, QList<ModResource>)> onComplete) {
    switch (platform) {
    case CurseForge: searchCurseForge(query, page, pageSize, onComplete); break;
    case Modrinth:   searchModrinth(query, page, pageSize, onComplete); break;
    }
}

void ModPlatform::searchByCategory(Platform platform, int category, int page,
                                     std::function<void(bool, QList<ModResource>)> onComplete) {
    // Category search delegates to regular search with category filter
    Q_UNUSED(category)
    searchMods(platform, QString(), page, 25, onComplete);
}

void ModPlatform::getModDetails(Platform platform, const QString &modId,
                                  std::function<void(bool, ModResource)> onComplete) {
    switch (platform) {
    case CurseForge: getCurseForgeModDetails(modId, onComplete); break;
    case Modrinth:   getModrinthModDetails(modId, onComplete); break;
    }
}

void ModPlatform::getModFiles(Platform platform, const QString &modId,
                                std::function<void(bool, QList<ModFileInfo>)> onComplete) {
    switch (platform) {
    case CurseForge: getCurseForgeFiles(modId, onComplete); break;
    case Modrinth:   getModrinthFiles(modId, onComplete); break;
    }
}

QString ModPlatform::getFileDownloadUrl(Platform platform,
                                          const QString &modId, const QString &fileId) {
    switch (platform) {
    case CurseForge:
        return QString("%1/mods/%2/files/%3/download-url").arg(CF_API, modId, fileId);
    case Modrinth:
        return QString("%1/project/%2/version/%3").arg(MR_API, modId, fileId);
    }
    return {};
}

void ModPlatform::downloadMod(Platform platform, const QString &modId,
                                const QString &fileId, const QString &savePath,
                                std::function<void(bool, QString)> onComplete,
                                std::function<void(qint64, qint64)> onProgress) {
    if (platform == CurseForge) {
        // CurseForge: first get the download URL, then download
        QString url = getFileDownloadUrl(platform, modId, fileId);
        QNetworkRequest req(url);
        req.setRawHeader("x-api-key", m_cfApiKey.toUtf8());
        req.setRawHeader("Accept", "application/json");

        DownloadManager::instance().downloadJson(
            url, [this, savePath, onComplete, onProgress](bool ok, QString, json result) {
                if (!ok) { onComplete(false, "Failed to get download URL"); return; }
                QString dlUrl = QString::fromStdString(result.value("data", ""));
                if (dlUrl.isEmpty()) { onComplete(false, "Empty download URL"); return; }

                DownloadManager::instance().download(dlUrl, savePath, onProgress, onComplete);
            });
    } else {
        // Modrinth: version endpoint returns file info with download URL
        QString url = getFileDownloadUrl(platform, modId, fileId);
        DownloadManager::instance().downloadJson(
            url, [savePath, onComplete, onProgress](bool ok, QString, json ver) {
                if (!ok) { onComplete(false, "Failed to get version info"); return; }
                if (!ver.contains("files") || ver["files"].empty()) {
                    onComplete(false, "No files in version"); return;
                }
                QString dlUrl = QString::fromStdString(ver["files"][0].value("url", ""));
                DownloadManager::instance().download(dlUrl, savePath, onProgress, onComplete);
            });
    }
}

// CurseForge API implementation

void ModPlatform::searchCurseForge(const QString &query, int page, int pageSize,
                                     std::function<void(bool, QList<ModResource>)> onComplete) {
    QUrl url(CF_API + "/mods/search");
    QUrlQuery q;
    q.addQueryItem("gameId", "432"); // Minecraft
    q.addQueryItem("classId", "6");  // Mods
    if (!query.isEmpty()) q.addQueryItem("searchFilter", query);
    q.addQueryItem("index", QString::number(page * pageSize));
    q.addQueryItem("pageSize", QString::number(pageSize));
    q.addQueryItem("sortField", "2"); // Popularity
    q.addQueryItem("sortOrder", "desc");
    url.setQuery(q);

    QNetworkRequest req(url);
    req.setRawHeader("x-api-key", m_cfApiKey.toUtf8());
    req.setRawHeader("Accept", "application/json");

    DownloadManager::instance().downloadJson(
        url.toString(), [onComplete](bool ok, QString, json result) {
            QList<ModResource> mods;
            if (!ok || !result.contains("data")) { onComplete(false, mods); return; }

            for (const auto &item : result["data"]) {
                ModResource r;
                r.id = QString::number(item.value("id", 0));
                r.name = QString::fromStdString(item.value("name", ""));
                r.summary = QString::fromStdString(item.value("summary", ""));
                r.author = QString::fromStdString(
                    item.value("authors", json::array()).empty() ? ""
                    : item["authors"][0].value("name", ""));
                r.iconUrl = QString::fromStdString(item.value("logo", json::object())
                    .value("thumbnailUrl", ""));
                r.downloadCount = item.value("downloadCount", 0);
                r.websiteUrl = QString::fromStdString(item.value("links", json::object())
                    .value("websiteUrl", ""));
                r.lastUpdated = 0;
                if (item.contains("latestFiles") && !item["latestFiles"].empty()) {
                    for (const auto &gv : item["latestFiles"][0].value("gameVersions", json::array())) {
                        r.versions.append(QString::fromStdString(gv.get<std::string>()));
                    }
                }
                mods.append(r);
            }
            onComplete(true, mods);
        });
}

void ModPlatform::getCurseForgeModDetails(const QString &modId,
                                            std::function<void(bool, ModResource)> onComplete) {
    QString url = CF_API + "/mods/" + modId;
    QNetworkRequest req(url);
    req.setRawHeader("x-api-key", m_cfApiKey.toUtf8());

    DownloadManager::instance().downloadJson(
        url, [onComplete](bool ok, QString, json result) {
            ModResource r;
            if (!ok || !result.contains("data")) { onComplete(false, r); return; }
            auto &item = result["data"];
            r.id = QString::number(item.value("id", 0));
            r.name = QString::fromStdString(item.value("name", ""));
            r.summary = QString::fromStdString(item.value("summary", ""));
            r.description = QString::fromStdString(item.value("description", ""));
            r.downloadCount = item.value("downloadCount", 0);
            onComplete(true, r);
        });
}

void ModPlatform::getCurseForgeFiles(const QString &modId,
                                       std::function<void(bool, QList<ModFileInfo>)> onComplete) {
    QString url = CF_API + "/mods/" + modId + "/files";
    QNetworkRequest req(url);
    req.setRawHeader("x-api-key", m_cfApiKey.toUtf8());

    DownloadManager::instance().downloadJson(
        url, [onComplete](bool ok, QString, json result) {
            QList<ModFileInfo> files;
            if (!ok || !result.contains("data")) { onComplete(false, files); return; }
            for (const auto &item : result["data"]) {
                ModFileInfo f;
                f.id = QString::number(item.value("id", 0));
                f.displayName = QString::fromStdString(item.value("displayName", ""));
                f.fileName = QString::fromStdString(item.value("fileName", ""));
                f.downloadUrl = QString::fromStdString(item.value("downloadUrl", ""));
                f.fileSize = item.value("fileLength", 0);
                f.sha1 = QString::fromStdString(
                    item.value("hashes", json::array()).empty() ? ""
                    : item["hashes"][0].value("value", ""));
                for (const auto &gv : item.value("gameVersions", json::array())) {
                    f.gameVersions.append(QString::fromStdString(gv.get<std::string>()));
                }
                files.append(f);
            }
            onComplete(true, files);
        });
}

// Modrinth API implementation

void ModPlatform::searchModrinth(const QString &query, int page, int pageSize,
                                   std::function<void(bool, QList<ModResource>)> onComplete) {
    QUrl url(MR_API + "/search");
    QUrlQuery q;
    if (!query.isEmpty()) q.addQueryItem("query", query);
    q.addQueryItem("offset", QString::number(page * pageSize));
    q.addQueryItem("limit", QString::number(pageSize));
    q.addQueryItem("facets", "[[\"project_type:mod\"]]");
    url.setQuery(q);

    DownloadManager::instance().downloadJson(
        url.toString(), [onComplete](bool ok, QString, json result) {
            QList<ModResource> mods;
            if (!ok || !result.contains("hits")) { onComplete(false, mods); return; }
            for (const auto &item : result["hits"]) {
                ModResource r;
                r.id = QString::fromStdString(item.value("project_id", ""));
                r.name = QString::fromStdString(item.value("title", ""));
                r.summary = QString::fromStdString(item.value("description", ""));
                r.author = QString::fromStdString(item.value("author", ""));
                r.iconUrl = QString::fromStdString(item.value("icon_url", ""));
                r.downloadCount = item.value("downloads", 0);
                r.websiteUrl = QString("https://modrinth.com/mod/%1").arg(r.id);
                if (item.contains("versions")) {
                    for (const auto &v : item["versions"]) {
                        r.versions.append(QString::fromStdString(v.get<std::string>()));
                    }
                }
                mods.append(r);
            }
            onComplete(true, mods);
        });
}

void ModPlatform::getModrinthModDetails(const QString &modId,
                                          std::function<void(bool, ModResource)> onComplete) {
    QString url = MR_API + "/project/" + modId;
    DownloadManager::instance().downloadJson(
        url, [onComplete](bool ok, QString, json item) {
            ModResource r;
            if (!ok) { onComplete(false, r); return; }
            r.id = QString::fromStdString(item.value("id", ""));
            r.name = QString::fromStdString(item.value("title", ""));
            r.summary = QString::fromStdString(item.value("description", ""));
            r.description = QString::fromStdString(item.value("body", ""));
            r.downloadCount = item.value("downloads", 0);
            r.websiteUrl = QString("https://modrinth.com/mod/%1").arg(r.id);
            onComplete(true, r);
        });
}

void ModPlatform::getModrinthFiles(const QString &modId,
                                     std::function<void(bool, QList<ModFileInfo>)> onComplete) {
    QString url = MR_API + "/project/" + modId + "/version";
    DownloadManager::instance().downloadJson(
        url, [onComplete](bool ok, QString, json result) {
            QList<ModFileInfo> files;
            if (!ok || !result.is_array()) { onComplete(false, files); return; }
            for (const auto &ver : result) {
                if (!ver.contains("files") || ver["files"].empty()) continue;
                ModFileInfo f;
                f.id = QString::fromStdString(ver.value("id", ""));
                f.displayName = QString::fromStdString(ver.value("name", ""));
                f.fileName = QString::fromStdString(ver["files"][0].value("filename", ""));
                f.downloadUrl = QString::fromStdString(ver["files"][0].value("url", ""));
                f.fileSize = ver["files"][0].value("size", 0);
                f.sha1 = QString::fromStdString(ver["files"][0].value("hashes", json::object())
                    .value("sha1", ""));
                for (const auto &gv : ver.value("game_versions", json::array())) {
                    f.gameVersions.append(QString::fromStdString(gv.get<std::string>()));
                }
                for (const auto &ld : ver.value("loaders", json::array())) {
                    f.loaders.append(QString::fromStdString(ld.get<std::string>()));
                }
                files.append(f);
            }
            onComplete(true, files);
        });
}
