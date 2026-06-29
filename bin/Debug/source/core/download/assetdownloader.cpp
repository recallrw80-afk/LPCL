#include "assetdownloader.h"
#include "downloadmanager.h"

#include <QLoggingCategory>

static Q_LOGGING_CATEGORY(logAsset, "pcl.asset")

AssetDownloader& AssetDownloader::instance()
{
    static AssetDownloader m;
    return m;
}

void AssetDownloader::downloadVersion(const QString &versionId,
                                       std::function<void(bool, QString)> onComplete)
{
    // TODO: Full pipeline in Phase 4
    // 1. Fetch version manifest -> get version JSON URL
    // 2. Download version JSON
    // 3. Download client JAR
    // 4. Parse asset index, download assets
    // 5. Parse libraries, download natives

    qCInfo(logAsset) << "Starting version download for:" << versionId;

    // For MVP, just fetch the version manifest and log the plan
    DownloadManager::instance().downloadJson(
        "https://launchermeta.mojang.com/mc/game/version_manifest.json",
        [versionId, onComplete](bool success, QString, nlohmann::json manifest) {
            if (!success) {
                if (onComplete) onComplete(false, "Failed to fetch version manifest");
                return;
            }
            // Find the version in the manifest
            if (manifest.contains("versions")) {
                for (const auto &v : manifest["versions"]) {
                    if (v.value("id", "") == versionId.toStdString()) {
                        QString url = QString::fromStdString(v.value("url", ""));
                        if (!url.isEmpty()) {
                            DownloadManager::instance().downloadJson(
                                url, [onComplete](bool ok, QString msg, nlohmann::json) {
                                    if (onComplete) onComplete(ok, msg);
                                });
                            return;
                        }
                    }
                }
            }
            if (onComplete) onComplete(false, "Version not found: " + versionId);
        });
}

void AssetDownloader::downloadVersionJson(const QString &versionId,
                                            std::function<void(bool, QString)> onComplete)
{
    downloadVersion(versionId, onComplete);
}

void AssetDownloader::downloadClientJar(const McVersion &version,
                                          std::function<void(bool, QString)> onComplete)
{
    // TODO: Download the version JAR from Mojang servers
    Q_UNUSED(version)
    qCInfo(logAsset) << "Client JAR download not yet implemented";
    if (onComplete) onComplete(false, "Not implemented yet");
}

void AssetDownloader::downloadAssets(const McVersion &version,
                                       std::function<void(bool, QString)> onComplete)
{
    Q_UNUSED(version)
    qCInfo(logAsset) << "Asset download not yet implemented";
    if (onComplete) onComplete(false, "Not implemented yet");
}

void AssetDownloader::downloadNatives(const McVersion &version,
                                        std::function<void(bool, QString)> onComplete)
{
    Q_UNUSED(version)
    qCInfo(logAsset) << "Native download not yet implemented";
    if (onComplete) onComplete(false, "Not implemented yet");
}
